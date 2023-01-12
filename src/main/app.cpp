#include "app.hpp"
#include <spdlog/spdlog.h>
#include <ctime>

namespace application {

AppStreamRecorder::AppStreamRecorder(codec::Transcoder& transcoder, codec::Writer& writer)
    : transcoder{transcoder}, writer{writer} {
}

AppStreamRecorder::~AppStreamRecorder() {
  transcoder.Flush(*this);
}

void AppStreamRecorder::ProcessBuffer(std::string_view buffer) {
  transcoder.Process(buffer, *this);
}

void AppStreamRecorder::ProcessEncodedData(AVPacket* packet) {
  writer.Process(packet);
}

AppStreamSubscriber::AppStreamSubscriber(AppLiveStreamOverseer& overseer, network::SenderNotifier& notifier)
    : overseer{overseer}, notifier{notifier} {
  overseer.AddSubscriber(this);
  spdlog::info("stream subscriber added");
  streamBuffer.emplace_back(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=FB\r\n\r\n");
}

AppStreamSubscriber::~AppStreamSubscriber() {
  overseer.RemoveSubscriber(this);
  spdlog::info("stream subscriber removed");
}

std::optional<std::string> AppStreamSubscriber::GetBuffered() {
  bool bufferEmpty{false};
  std::optional<std::string> result;
  {
    std::lock_guard lock{bufferMut};
    if (streamBuffer.empty()) {
      return "";
    }
    result = streamBuffer.front();
    streamBuffer.pop_front();
    bufferEmpty = streamBuffer.empty();
  }
  if (bufferEmpty) {
    notifier.UnmarkPending();
  }
  return result;
}

void AppStreamSubscriber::ProcessFrame(std::string_view frame) {
  std::string buf = "--FB\r\nContent-Type: image/jpeg\r\n\r\n";
  buf += frame;
  buf += "\r\n\r\n";
  {
    std::lock_guard lock{bufferMut};
    streamBuffer.emplace_back(std::move(buf));
  }
  notifier.MarkPending();
}

AppStreamSubscriberFactory::AppStreamSubscriberFactory(AppLiveStreamOverseer& overseer) : overseer{overseer} {
}

std::unique_ptr<network::RawStream> AppStreamSubscriberFactory::GetStream(network::SenderNotifier& notifier) {
  return std::make_unique<AppStreamSubscriber>(overseer, notifier);
}

void AppLiveStreamOverseer::ProcessBuffer(std::string_view buffer) {
  NotifySubscribers(buffer);
  SaveSnapshot(buffer);
}

void AppLiveStreamOverseer::AddSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.emplace(subscriber);
}

void AppLiveStreamOverseer::RemoveSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.erase(subscriber);
}

void AppLiveStreamOverseer::SaveSnapshot(std::string_view frame) {
  std::lock_guard lock{snapshotMut};
  snapshot = frame;
}

std::string AppLiveStreamOverseer::GetSnapshot() const {
  std::lock_guard lock{snapshotMut};
  std::string copy{snapshot};
  return copy;
}

void AppLiveStreamOverseer::NotifySubscribers(std::string_view frame) const {
  std::lock_guard lock{subscribersMut};
  for (auto* s : subscribers) {
    s->ProcessFrame(frame);
  }
}

AppStreamProcessor::AppStreamProcessor(video::StreamOptions&& streamOptions, AppLiveStreamOverseer& liveStreamOverseer,
                                       codec::DecoderOptions&& decoderOptions, codec::FilterOptions&& filterOptions,
                                       codec::EncoderOptions&& encoderOptions, codec::WriterOptions&& writerOptions)
    : liveStreamOverseer{liveStreamOverseer},
      decoderOptions{std::move(decoderOptions)},
      filterOptions{std::move(filterOptions)},
      encoderOptions{std::move(encoderOptions)},
      writerOptions{std::move(writerOptions)} {
  StartStreaming(std::move(streamOptions));
}

void AppStreamProcessor::StartStreaming(video::StreamOptions&& streamOptions) {
  streamThread = std::thread([this, streamOptions = std::move(streamOptions)]() mutable {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(std::move(streamOptions));
    while (true) {
      spdlog::debug("processing new frame");
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamProcessor::StartRecording() {
  StopRecording();
  if (recorderThread.joinable()) {
    recorderThread.join();
  }
  {
    std::lock_guard lock{recorderMut};
    recorderRunning = true;
  }
  recorderThread = std::thread([this, decoderOptions = decoderOptions, filterOptions = filterOptions,
                                encoderOptions = encoderOptions, writerOptions = writerOptions]() mutable {
    codec::Decoder decoder{std::move(decoderOptions)};
    codec::Filter filter{std::move(filterOptions)};
    codec::Encoder encoder{std::move(encoderOptions)};
    codec::Transcoder transcoder{decoder, filter, encoder};
    std::time_t tm = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&tm));
    codec::Writer writer{buf, std::move(writerOptions)};
    AppStreamRecorder recorder{transcoder, writer};
    std::unique_lock lock{recorderMut};
    recorderBuffer.clear();
    while (recorderRunning) {
      recorderCv.wait(lock, [this] { return not recorderRunning or not recorderBuffer.empty(); });
      while (not recorderBuffer.empty()) {
        auto buffer = recorderBuffer.front();
        recorderBuffer.pop_front();
        recorder.ProcessBuffer(buffer);
      }
    }
  });
}

void AppStreamProcessor::StopRecording() {
  std::unique_lock lock{recorderMut};
  recorderRunning = false;
  lock.unlock();
  recorderCv.notify_one();
}

bool AppStreamProcessor::IsRecording() const {
  std::lock_guard lock{recorderMut};
  return recorderRunning;
}

void AppStreamProcessor::ProcessFrame(std::string_view frame) {
  liveStreamOverseer.ProcessBuffer(frame);
  {
    std::unique_lock lock{recorderMut};
    if (recorderRunning) {
      recorderBuffer.emplace_back(frame);
      lock.unlock();
      recorderCv.notify_one();
    }
  }
}

AppLayer::AppLayer(AppStreamProcessor& streamProcessor, AppLiveStreamOverseer& liveStreamOverseer)
    : streamProcessor{streamProcessor}, liveStreamOverseer{liveStreamOverseer} {
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
    return network::RawStreamHttpResponse{std::make_unique<AppStreamSubscriberFactory>(liveStreamOverseer)};
  }
  if (req.uri == "/snapshot") {
    const auto payload = liveStreamOverseer.GetSnapshot();
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::OK;
    resp.headers.emplace("Content-Type", "image/jpeg");
    resp.body = std::move(payload);
    return resp;
  }
  if (req.uri == "/recording") {
    return BuildPlainTextRequest(network::HttpStatus::OK, streamProcessor.IsRecording() ? "yes" : "no");
  }
  if (req.uri == "/control") {
    const auto recordingControl = req.query.find("recording");
    if (recordingControl != req.query.cend()) {
      if (recordingControl->second == "on") {
        streamProcessor.StartRecording();
      }
      if (recordingControl->second == "off") {
        streamProcessor.StopRecording();
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
