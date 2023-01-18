#include "app.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"
#include "stream.hpp"

namespace application {

AppLowFrameRateMjpegSender::AppLowFrameRateMjpegSender(
    network::HttpSender& sender, AppStreamDistributer& mjpegDistributer)
    : sender{sender}, mjpegDistributer{mjpegDistributer} {
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
    network::HttpSender& sender, AppStreamDistributer& mjpegDistributer)
    : sender{sender}, mjpegDistributer{mjpegDistributer} {
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

AppStreamMpegTsSender::AppStreamMpegTsSender(
    network::HttpSender& sender, AppStreamDistributer& mjpegDistributer, AppStreamTranscoderFactory& transcoderFactory)
    : sender{sender}, mjpegDistributer{mjpegDistributer}, transcoder{transcoderFactory.Create(*this)} {
  mjpegDistributer.AddSubscriber(this);
  network::ChunkedHeaderHttpResponse resp;
  resp.headers.emplace("Content-Type", "video/mpegts");
  sender.Send(std::move(resp));
}

AppStreamMpegTsSender::~AppStreamMpegTsSender() {
  mjpegDistributer.RemoveSubscriber(this);
  transcoder.reset();
}

void AppStreamMpegTsSender::Notify(std::string_view buffer) {
  transcoder->Process(buffer);
}

void AppStreamMpegTsSender::WriteData(std::string_view buffer) {
  std::string body{buffer};
  sender.Send(network::ChunkedDataHttpResponse{std::move(body)});
}

AppStreamSnapshotSaver::AppStreamSnapshotSaver(AppStreamDistributer& distributer) : distributer{distributer} {
  distributer.AddSubscriber(this);
}

AppStreamSnapshotSaver::~AppStreamSnapshotSaver() {
  distributer.RemoveSubscriber(this);
}

void AppStreamSnapshotSaver::Notify(std::string_view buffer) {
  std::lock_guard lock{snapshotMut};
  snapshot = buffer;
}

std::string AppStreamSnapshotSaver::GetSnapshot() const {
  std::lock_guard lock{snapshotMut};
  return snapshot;
}

AppStreamRecorderController::AppStreamRecorderController(
    AppStreamDistributer& streamDistributer, common::EventQueue<AppRecorderEvent>& eventQueue)
    : streamDistributer{streamDistributer}, eventQueue{eventQueue} {
  streamDistributer.AddSubscriber(this);
}

AppStreamRecorderController::~AppStreamRecorderController() {
  streamDistributer.RemoveSubscriber(this);
}

void AppStreamRecorderController::Notify(std::string_view buffer) {
  std::lock_guard lock{confMut};
  if (not isRecording) {
    return;
  }
  eventQueue.Push(RecordData{std::string{buffer}});
}

void AppStreamRecorderController::Start() {
  std::lock_guard lock{confMut};
  isRecording = true;
  eventQueue.Push(StartRecording{});
}

void AppStreamRecorderController::Stop() {
  std::lock_guard lock{confMut};
  isRecording = false;
  eventQueue.Push(StopRecording{});
}

bool AppStreamRecorderController::IsRecording() const {
  std::lock_guard lock{confMut};
  return isRecording;
}

AppLayer::AppLayer(network::HttpSender& sender, AppStreamDistributer& mjpegDistributer,
    AppStreamSnapshotSaver& snapshotSaver, AppStreamRecorderController& recorderController,
    AppStreamTranscoderFactory& transcoderFactory)
    : sender{sender},
      mjpegDistributer{mjpegDistributer},
      snapshotSaver{snapshotSaver},
      processorController{recorderController},
      transcoderFactory{transcoderFactory} {
}

AppLayer::~AppLayer() {
  streamSender.reset();
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
    streamSender = std::make_unique<AppLowFrameRateMjpegSender>(sender, mjpegDistributer);
    return;
  }
  if (req.uri == "/mjpeg2") {
    streamSender = std::make_unique<AppHighFrameRateMjpegSender>(sender, mjpegDistributer);
    return;
  }
  if (req.uri == "/mpegts") {
    streamSender = std::make_unique<AppStreamMpegTsSender>(sender, mjpegDistributer, transcoderFactory);
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
        processorController.Start();
      }
      if (recordingControl->second == "off") {
        processorController.Stop();
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
    AppStreamRecorderController& recorderController, AppStreamTranscoderFactory& transcoderFactory)
    : mjpegDistributer{mjpegDistributer},
      snapshotSaver{snapshotSaver},
      recorderController{recorderController},
      transcoderFactory{transcoderFactory} {
}

std::unique_ptr<network::HttpProcessor> AppLayerFactory::Create(network::HttpSender& sender) const {
  return std::make_unique<AppLayer>(sender, mjpegDistributer, snapshotSaver, recorderController, transcoderFactory);
}

}  // namespace application
