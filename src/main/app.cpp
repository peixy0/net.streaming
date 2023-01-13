#include "app.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppLayer::AppLayer(AppStreamDistributer& streamDistributer, common::EventQueue<RecorderEvent>& recorderEventQueue)
    : streamDistributer{streamDistributer}, recorderEventQueue{recorderEventQueue} {
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  if (req.uri == "/") {
    network::FileHttpResponse resp;
    resp.path = "index.html";
    resp.headers.emplace("Content-Type", "text/html");
    return resp;
  }
  if (req.uri == "/stream") {
    return network::RawStreamHttpResponse{std::make_unique<AppStreamSubscriberFactory>(streamDistributer)};
  }
  if (req.uri == "/snapshot") {
    const auto payload = streamDistributer.GetSnapshot();
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::OK;
    resp.headers.emplace("Content-Type", "image/jpeg");
    resp.body = std::move(payload);
    return resp;
  }
  if (req.uri == "/recording") {
    return BuildPlainTextRequest(network::HttpStatus::OK, isRecording ? "yes" : "no");
  }
  if (req.uri == "/control") {
    const auto recordingControl = req.query.find("recording");
    if (recordingControl != req.query.cend()) {
      if (recordingControl->second == "on") {
        isRecording = true;
        recorderEventQueue.Push(StartRecording{});
      }
      if (recordingControl->second == "off") {
        isRecording = false;
        recorderEventQueue.Push(StopRecording{});
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
