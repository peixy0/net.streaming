#include "app.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppLowFrameRateMjpegSender::AppLowFrameRateMjpegSender(
    AppStreamDistributer& mjpegDistributer, network::HttpSender& sender)
    : mjpegDistributer{mjpegDistributer}, sender{sender} {
  sender.Send(network::MixedReplaceHeaderHttpResponse{});
  mjpegDistributer.AddSubscriber(this);
}

AppLowFrameRateMjpegSender::~AppLowFrameRateMjpegSender() {
  mjpegDistributer.RemoveSubscriber(this);
}

void AppLowFrameRateMjpegSender::Notify(std::string_view buffer) {
  if (skipped++ < 1) {
    return;
  }
  skipped = 0;
  network::MixedReplaceDataHttpResponse resp;
  resp.headers.emplace("Content-Type", "image/jpeg");
  resp.body = buffer;
  return sender.Send(std::move(resp));
}

AppHighFrameRateMjpegSender::AppHighFrameRateMjpegSender(
    AppStreamDistributer& mjpegDistributer, network::HttpSender& sender)
    : mjpegDistributer{mjpegDistributer}, sender{sender} {
  sender.Send(network::MixedReplaceHeaderHttpResponse{});
  mjpegDistributer.AddSubscriber(this);
}

AppHighFrameRateMjpegSender::~AppHighFrameRateMjpegSender() {
  mjpegDistributer.RemoveSubscriber(this);
}

void AppHighFrameRateMjpegSender::Notify(std::string_view buffer) {
  network::MixedReplaceDataHttpResponse resp;
  resp.headers.emplace("Content-Type", "image/jpeg");
  resp.body = buffer;
  return sender.Send(std::move(resp));
}

AppStreamProcessorController::AppStreamProcessorController(common::EventQueue<StreamProcessorEvent>& eventQueue)
    : eventQueue{eventQueue} {
}

void AppStreamProcessorController::SetRecording(bool value) {
  std::lock_guard lock{confMut};
  isRecording = value;
  if (isRecording) {
    eventQueue.Push(RecordingStart{});
  } else {
    eventQueue.Push(RecordingStop{});
  }
}

bool AppStreamProcessorController::IsRecording() const {
  std::lock_guard lock{confMut};
  return isRecording;
}

AppLayer::AppLayer(network::HttpSender& sender, AppStreamDistributer& mjpegDistributer,
    AppStreamSnapshotSaver& snapshotSaver, AppStreamProcessorController& processorController)
    : sender{sender},
      mjpegDistributer{mjpegDistributer},
      snapshotSaver{snapshotSaver},
      processorController{processorController} {
}

AppLayer::~AppLayer() {
}

void AppLayer::Process(network::HttpRequest&& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  if (req.uri == "/") {
    network::FileHttpResponse resp;
    resp.path = "index.html";
    resp.headers.emplace("Content-Type", "text/html");
    return sender.Send(std::move(resp));
  }
  if (req.uri == "/snapshot") {
    const auto payload = snapshotSaver.GetSnapshot();
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::OK;
    resp.headers.emplace("Content-Type", "image/jpeg");
    resp.body = std::move(payload);
    return sender.Send(std::move(resp));
  }
  if (req.uri == "/mjpeg") {
    streamReceiver = std::make_unique<AppLowFrameRateMjpegSender>(mjpegDistributer, sender);
    return;
  }
  if (req.uri == "/mjpeg2") {
    streamReceiver = std::make_unique<AppHighFrameRateMjpegSender>(mjpegDistributer, sender);
    return;
  }
  if (req.uri == "/recording") {
    return sender.Send(
        BuildPlainTextRequest(network::HttpStatus::OK, processorController.IsRecording() ? "yes" : "no"));
  }
  if (req.uri == "/control") {
    const auto recordingControl = req.query.find("recording");
    if (recordingControl != req.query.cend()) {
      if (recordingControl->second == "on") {
        processorController.SetRecording(true);
      }
      if (recordingControl->second == "off") {
        processorController.SetRecording(false);
      }
    }
    return sender.Send(BuildPlainTextRequest(network::HttpStatus::OK, "OK"));
  }
  return sender.Send(BuildPlainTextRequest(network::HttpStatus::NotFound, "Not Found"));
}

network::PreparedHttpResponse AppLayer::BuildPlainTextRequest(network::HttpStatus status, std::string_view body) const {
  network::PreparedHttpResponse resp;
  resp.status = status;
  resp.headers.emplace("Content-Type", "text/plain; charset=UTF-8");
  resp.body = body;
  return resp;
}

AppLayerFactory::AppLayerFactory(AppStreamDistributer& mjpegDistributer, AppStreamSnapshotSaver& snapshotSaver,
    AppStreamProcessorController& configuration)
    : mjpegDistributer{mjpegDistributer}, snapshotSaver{snapshotSaver}, processorController{configuration} {
}

std::unique_ptr<network::HttpProcessor> AppLayerFactory::Create(network::HttpSender& sender) const {
  return std::make_unique<AppLayer>(sender, mjpegDistributer, snapshotSaver, processorController);
}

}  // namespace application
