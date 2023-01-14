#include "stream.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace application {

AppStreamProcessorRunner::AppStreamProcessorRunner(common::EventQueue<StreamProcessorEvent>& eventQueue,
    application::AppStreamDistributer& mjpegDistributer, application::AppStreamDistributer& streamDistributer,
    const AppStreamProcessorOptions& processorOptions, const codec::DecoderOptions& decoderOptions,
    const codec::FilterOptions& filterOptions, const codec::EncoderOptions& encoderOptions,
    const codec::WriterOptions& writerOptions)
    : eventQueue{eventQueue},
      mjpegDistributer{mjpegDistributer},
      h264Distributer{streamDistributer},
      processorOptions{processorOptions},
      decoderOptions{decoderOptions},
      filterOptions{filterOptions},
      encoderOptions{encoderOptions},
      writerOptions{writerOptions} {
  Reset();
}

void AppStreamProcessorRunner::Run() {
  processorThread = std::thread([this] {
    while (true) {
      auto event = eventQueue.Pop();
      std::visit(*this, event);
    }
  });
}

void AppStreamProcessorRunner::Process(std::string_view buffer) {
  if (processorOptions.distributeMjpeg) {
    mjpegDistributer.Process(buffer);
  }
  if (processorOptions.saveRecord and processorOptions.maxRecordingTimeInSeconds > 0) {
    const auto now = std::time(nullptr);
    const auto diff = std::difftime(now, recorderStartTime);
    if (diff >= processorOptions.maxRecordingTimeInSeconds) {
      StartRecording();
    }
  }
  if (processorOptions.distributeH264 or processorOptions.saveRecord) {
    transcoder->Process(buffer, *this);
  }
}

void AppStreamProcessorRunner::ProcessEncodedData(AVPacket* encoded) {
  if (processorOptions.distributeH264) {
    const char* p = reinterpret_cast<const char*>(encoded->data);
    h264Distributer.Process({p, p + encoded->size});
  }
  if (processorOptions.saveRecord) {
    writer->Process(encoded);
  }
}

void AppStreamProcessorRunner::Reset() {
  transcoder.reset();
  encoder.reset();
  filter.reset();
  decoder.reset();
  writer.reset();
  if (processorOptions.distributeH264 or processorOptions.saveRecord) {
    decoder = std::make_unique<codec::Decoder>(decoderOptions);
    filter = std::make_unique<codec::Filter>(filterOptions);
    encoder = std::make_unique<codec::Encoder>(encoderOptions);
    transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  }
  if (processorOptions.saveRecord) {
    recorderStartTime = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&recorderStartTime));
    writer = std::make_unique<codec::Writer>(buf, writerOptions);
  }
}

void AppStreamProcessorRunner::StartRecording() {
  processorOptions.saveRecord = true;
  Reset();
}

void AppStreamProcessorRunner::StopRecording() {
  processorOptions.saveRecord = false;
  Reset();
}

void AppStreamProcessorRunner::operator()(const RecordingStart&) {
  StartRecording();
}

void AppStreamProcessorRunner::operator()(const RecordingStop&) {
  StopRecording();
}

void AppStreamProcessorRunner::operator()(const ProcessBuffer& recordBuffer) {
  Process(recordBuffer.buffer);
}

void AppStreamSnapshotSaver::Notify(std::string_view buffer) {
  std::lock_guard lock{snapshotMut};
  snapshot = buffer;
}

AppStreamSnapshotSaver::AppStreamSnapshotSaver(AppStreamDistributer& distributer) : distributer{distributer} {
  distributer.AddSubscriber(this);
}

AppStreamSnapshotSaver::~AppStreamSnapshotSaver() {
  distributer.RemoveSubscriber(this);
}

std::string AppStreamSnapshotSaver::GetSnapshot() const {
  std::lock_guard lock{snapshotMut};
  return snapshot;
}

AppStream::AppStream(AppStreamDistributer& distributer, network::SenderNotifier& notifier)
    : distributer{distributer}, notifier{notifier} {
}

std::optional<std::string> AppStream::GetBuffered() {
  std::string result;
  {
    std::lock_guard lock{bufferMut};
    while (not streamBuffer.empty()) {
      result += streamBuffer.front();
      streamBuffer.pop_front();
    }
    notifier.UnmarkPending();
  }
  return result;
}

AppMjpegStream::AppMjpegStream(AppStreamDistributer& distributer, network::SenderNotifier& notifier)
    : AppStream{distributer, notifier} {
  distributer.AddSubscriber(this);
  spdlog::info("mjpeg stream added");
  streamBuffer.emplace_back(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=FB\r\n\r\n");
}

AppMjpegStream::~AppMjpegStream() {
  distributer.RemoveSubscriber(this);
  spdlog::info("mjpeg stream removed");
}

void AppMjpegStream::Notify(std::string_view buffer) {
  static constexpr int skipBufferSize = 1;
  if (skipped++ < skipBufferSize) {
    return;
  }
  skipped = 0;

  static constexpr int maxBufferSize = 30;
  std::string buf = "--FB\r\nContent-Type: image/jpeg\r\n\r\n";
  buf += buffer;
  buf += "\r\n\r\n";
  {
    std::lock_guard lock{bufferMut};
    if (streamBuffer.size() < maxBufferSize) {
      streamBuffer.emplace_back(std::move(buf));
    }
    notifier.MarkPending();
  }
}

AppH264Stream::AppH264Stream(AppStreamDistributer& distributer, network::SenderNotifier& notifier)
    : AppStream{distributer, notifier} {
  distributer.AddSubscriber(this);
  spdlog::info("H264 stream added");
  streamBuffer.emplace_back(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: video/H264\r\n"
      "Transfer-Encoding: chunked\r\n\r\n");
}

AppH264Stream::~AppH264Stream() {
  distributer.RemoveSubscriber(this);
  spdlog::info("H264 stream removed");
}

void AppH264Stream::Notify(std::string_view frame) {
  static constexpr int maxBufferSize = 30;
  std::stringstream ss;
  ss << std::hex << frame.size();
  ss << "\r\n";
  ss << frame;
  ss << "\r\n";
  {
    std::lock_guard lock{bufferMut};
    if (streamBuffer.size() < maxBufferSize) {
      streamBuffer.emplace_back(ss.str());
    }
    notifier.MarkPending();
  }
}

AppMjpegStreamFactory::AppMjpegStreamFactory(AppStreamDistributer& distributer) : distributer{distributer} {
}

std::unique_ptr<network::RawStream> AppMjpegStreamFactory::GetStream(network::SenderNotifier& notifier) {
  return std::make_unique<AppMjpegStream>(distributer, notifier);
}

AppH264StreamFactory::AppH264StreamFactory(AppStreamDistributer& distributer) : distributer{distributer} {
}

std::unique_ptr<network::RawStream> AppH264StreamFactory::GetStream(network::SenderNotifier& notifier) {
  return std::make_unique<AppH264Stream>(distributer, notifier);
}

void AppStreamDistributer::Process(std::string_view buffer) {
  std::lock_guard lock{receiversMut};
  for (auto* s : receivers) {
    s->Notify(buffer);
  }
}

void AppStreamDistributer::AddSubscriber(AppStreamReceiver* subscriber) {
  std::lock_guard lock{receiversMut};
  receivers.emplace(subscriber);
}

void AppStreamDistributer::RemoveSubscriber(AppStreamReceiver* subscriber) {
  std::lock_guard lock{receiversMut};
  receivers.erase(subscriber);
}

AppStreamCapturerRunner::AppStreamCapturerRunner(
    const video::StreamOptions& streamOptions, common::EventQueue<StreamProcessorEvent>& processorEventQueue)
    : streamOptions{streamOptions}, processorEventQueue{processorEventQueue} {
}

void AppStreamCapturerRunner::Run() {
  streamThread = std::thread([this] {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(streamOptions);
    while (true) {
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamCapturerRunner::ProcessFrame(std::string_view frame) {
  processorEventQueue.Push(ProcessBuffer{{frame.data(), frame.size()}});
}

}  // namespace application
