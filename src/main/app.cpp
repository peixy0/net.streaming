#include "app.hpp"
#include <spdlog/spdlog.h>

namespace {

network::HttpResponse BuildPlainTextRequest(network::HttpStatus status, std::string_view body) {
  network::HttpResponse resp;
  resp.status = status;
  resp.headers.emplace("Content-Type", "text/plain; charset=UTF-8");
  resp.body = body;
  return resp;
}

}  // namespace

namespace application {

AppMjpegSender::AppMjpegSender(AppStreamDistributer& mjpegDistributer, network::HttpSender& sender)
    : mjpegDistributer{mjpegDistributer}, sender{sender} {
}

AppMjpegSender::~AppMjpegSender() {
  mjpegDistributer.RemoveSubscriber(this);
}

void AppMjpegSender::Process(network::HttpRequest&&) {
  sender.Send(network::MixedReplaceHeaderHttpResponse{});
  mjpegDistributer.AddSubscriber(this);
}

AppLowFrameRateMjpegSender::AppLowFrameRateMjpegSender(
    AppStreamDistributer& mjpegDistributer, network::HttpSender& sender)
    : AppMjpegSender{mjpegDistributer, sender} {
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

AppLowFrameRateMjpegSenderFactory::AppLowFrameRateMjpegSenderFactory(AppStreamDistributer& distributer)
    : distributer{distributer} {
}

std::unique_ptr<network::HttpProcessor> AppLowFrameRateMjpegSenderFactory::Create(network::HttpSender& sender) const {
  return std::make_unique<AppLowFrameRateMjpegSender>(distributer, sender);
}

AppHighFrameRateMjpegSender::AppHighFrameRateMjpegSender(
    AppStreamDistributer& mjpegDistributer, network::HttpSender& sender)
    : AppMjpegSender{mjpegDistributer, sender} {
}

void AppHighFrameRateMjpegSender::Notify(std::string_view buffer) {
  network::MixedReplaceDataHttpResponse resp;
  resp.headers.emplace("Content-Type", "image/jpeg");
  resp.body = buffer;
  return sender.Send(std::move(resp));
}

AppHighFrameRateMjpegSenderFactory::AppHighFrameRateMjpegSenderFactory(AppStreamDistributer& distributer)
    : distributer{distributer} {
}

std::unique_ptr<network::HttpProcessor> AppHighFrameRateMjpegSenderFactory::Create(network::HttpSender& sender) const {
  return std::make_unique<AppHighFrameRateMjpegSender>(distributer, sender);
}

AppEncodedStreamSender::AppEncodedStreamSender(
    AppStreamDistributer& mjpegDistributer, AppStreamTranscoderFactory& transcoderFactory, network::HttpSender& sender)
    : mjpegDistributer{mjpegDistributer}, transcoder{transcoderFactory.Create(*this)}, sender{sender} {
  transcoderThread = std::thread([this] { RunTranscoder(); });
  senderThread = std::thread([this] { RunSender(); });
}

AppEncodedStreamSender::~AppEncodedStreamSender() {
  mjpegDistributer.RemoveSubscriber(this);
  transcoderQueue.Push(std::nullopt);
  senderQueue.Push(std::nullopt);
  transcoderThread.join();
  senderThread.join();
  transcoder.reset();
}

void AppEncodedStreamSender::Notify(std::string_view buffer) {
  transcoderQueue.Push(std::make_optional<std::string>(buffer));
}

void AppEncodedStreamSender::WriteData(std::string_view buffer) {
  senderQueue.Push(std::make_optional<std::string>(buffer));
}

void AppEncodedStreamSender::Process(network::HttpRequest&&) {
  network::ChunkedHeaderHttpResponse resp;
  sender.Send(std::move(resp));
  mjpegDistributer.AddSubscriber(this);
}

void AppEncodedStreamSender::RunTranscoder() {
  std::optional<std::string> bufferOpt;
  while ((bufferOpt = transcoderQueue.Pop()) != std::nullopt) {
    if (transcoderQueue.Size() > 0) {
      continue;
    }
    transcoder->Process(std::move(*bufferOpt));
  }
}

void AppEncodedStreamSender::RunSender() {
  std::optional<std::string> bufferOpt;
  std::string buffer;
  constexpr int batchedBufferSize = 1 << 18;
  buffer.reserve(batchedBufferSize);
  while ((bufferOpt = senderQueue.Pop()) != std::nullopt) {
    if (buffer.size() + bufferOpt->size() <= batchedBufferSize) {
      buffer += std::move(*bufferOpt);
      continue;
    }
    sender.Send(network::ChunkedDataHttpResponse{std::move(buffer)});
    buffer.clear();
    buffer.reserve(batchedBufferSize);
    buffer += std::move(*bufferOpt);
  }
}

AppEncodedStreamSenderFactory::AppEncodedStreamSenderFactory(
    AppStreamDistributer& distributer, AppStreamTranscoderFactory& transcoderFactory)
    : distributer{distributer}, transcoderFactory{transcoderFactory} {
}

std::unique_ptr<network::HttpProcessor> AppEncodedStreamSenderFactory::Create(network::HttpSender& sender) const {
  return std::make_unique<AppEncodedStreamSender>(distributer, transcoderFactory, sender);
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

AppHttpLayer::AppHttpLayer(AppStreamSnapshotSaver& snapshotSaver, AppStreamRecorderController& recorderController)
    : snapshotSaver{snapshotSaver}, processorController{recorderController} {
}

void AppHttpLayer::GetIndex(network::HttpRequest&&, network::HttpSender& sender) const {
  network::FileHttpResponse resp;
  resp.path = "index.html";
  resp.headers.emplace("Content-Type", "text/html");
  return sender.Send(std::move(resp));
}

void AppHttpLayer::GetSnapshot(network::HttpRequest&&, network::HttpSender& sender) const {
  const auto payload = snapshotSaver.GetSnapshot();
  network::HttpResponse resp;
  resp.status = network::HttpStatus::OK;
  resp.headers.emplace("Content-Type", "image/jpeg");
  resp.body = std::move(payload);
  return sender.Send(std::move(resp));
}

void AppHttpLayer::GetRecording(network::HttpRequest&&, network::HttpSender& sender) const {
  return sender.Send(BuildPlainTextRequest(network::HttpStatus::OK, processorController.IsRecording() ? "on" : "off"));
}

void AppHttpLayer::SetRecording(network::HttpRequest&& req, network::HttpSender& sender) const {
  if (req.body == "on") {
    processorController.Start();
  }
  if (req.body == "off") {
    processorController.Stop();
  }
  return sender.Send(BuildPlainTextRequest(network::HttpStatus::OK, "OK"));
}

}  // namespace application
