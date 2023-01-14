#include "app.hpp"
#include <spdlog/spdlog.h>
#include "stream.hpp"

namespace application {

AppLayer::AppLayer(AppStreamDistributer& streamDistributer, AppStreamDistributer& h264Distributer,
    AppStreamSnapshotSaver& snapshotSaver, common::EventQueue<StreamProcessorEvent>& processorEventQueue)
    : mjpegDistributer{streamDistributer},
      h264Distributer{h264Distributer},
      snapshotSaver{snapshotSaver},
      streamProcessorEventQueue{processorEventQueue} {
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  if (req.uri == "/") {
    network::FileHttpResponse resp;
    resp.path = "index.html";
    resp.headers.emplace("Content-Type", "text/html");
    return resp;
  }
  if (req.uri == "/snapshot") {
    const auto payload = snapshotSaver.GetSnapshot();
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::OK;
    resp.headers.emplace("Content-Type", "image/jpeg");
    resp.body = std::move(payload);
    return resp;
  }
  if (req.uri == "/mjpeg") {
    return network::RawStreamHttpResponse{std::make_unique<AppMjpegStreamFactory>(mjpegDistributer)};
  }
  if (req.uri == "/h264") {
    return network::RawStreamHttpResponse{std::make_unique<AppH264StreamFactory>(h264Distributer)};
  }
  if (req.uri == "/recording") {
    return BuildPlainTextRequest(network::HttpStatus::OK, isRecording ? "yes" : "no");
  }
  if (req.uri == "/control") {
    const auto recordingControl = req.query.find("recording");
    if (recordingControl != req.query.cend()) {
      if (recordingControl->second == "on") {
        isRecording = true;
        streamProcessorEventQueue.Push(RecordingStart{});
      }
      if (recordingControl->second == "off") {
        isRecording = false;
        streamProcessorEventQueue.Push(RecordingStop{});
      }
    }
    return BuildPlainTextRequest(network::HttpStatus::OK, "OK");
  }
  return BuildPlainTextRequest(network::HttpStatus::NotFound, "Not Found");
}

network::HttpResponse AppLayer::BuildPlainTextRequest(network::HttpStatus status, std::string_view body) const {
  network::PreparedHttpResponse resp;
  resp.status = status;
  resp.headers.emplace("Content-Type", "text/plain; charset=UTF-8");
  resp.body = body;
  return resp;
}

}  // namespace application
