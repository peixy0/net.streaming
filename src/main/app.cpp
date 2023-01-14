#include "app.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppLayer::AppLayer(AppStreamDistributer& streamDistributer, AppStreamDistributer& h264Distributer,
    AppStreamSnapshotSaver& snapshotSaver, const AppStreamProcessorOptions& streamProcessorOptions,
    common::EventQueue<StreamProcessorEvent>& processorEventQueue)
    : mjpegStreamFactory{streamDistributer},
      h264StreamFactory{h264Distributer},
      snapshotSaver{snapshotSaver},
      streamProcessorOptions{streamProcessorOptions},
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
  if (streamProcessorOptions.distributeMjpeg and req.uri == "/mjpeg") {
    return network::RawStreamHttpResponse{mjpegStreamFactory};
  }
  if (streamProcessorOptions.distributeH264 and req.uri == "/h264") {
    return network::RawStreamHttpResponse{h264StreamFactory};
  }
  if (req.uri == "/recording") {
    return BuildPlainTextRequest(network::HttpStatus::OK, streamProcessorOptions.saveRecord ? "yes" : "no");
  }
  if (req.uri == "/control") {
    const auto recordingControl = req.query.find("recording");
    if (recordingControl != req.query.cend()) {
      if (recordingControl->second == "on") {
        streamProcessorOptions.saveRecord = true;
        streamProcessorEventQueue.Push(RecordingStart{});
      }
      if (recordingControl->second == "off") {
        streamProcessorOptions.saveRecord = false;
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
