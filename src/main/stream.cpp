#include "stream.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace application {

AppStreamProcessorRunner::AppStreamProcessorRunner(common::EventQueue<StreamProcessorEvent>& eventQueue,
    application::AppStreamDistributer& mjpegDistributer, const AppStreamProcessorOptions& processorOptions,
    const codec::DecoderOptions& decoderOptions, const codec::FilterOptions& filterOptions,
    const codec::EncoderOptions& encoderOptions, const codec::WriterOptions& writerOptions)
    : eventQueue{eventQueue},
      mjpegDistributer{mjpegDistributer},
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
  if (processorOptions.saveRecord) {
    if (processorOptions.maxRecordingTimeInSeconds > 0) {
      const auto now = std::time(nullptr);
      const auto diff = std::difftime(now, recorderStartTime);
      if (diff >= processorOptions.maxRecordingTimeInSeconds) {
        Reset();
      }
    }
    transcoder->Process(buffer, *this);
  }
}

void AppStreamProcessorRunner::ProcessEncodedData(AVPacket* encoded) {
  if (processorOptions.saveRecord) {
    writer->Process(encoded);
  }
}

void AppStreamProcessorRunner::Reset() {
  ResetWriter();
  encoder.reset();
  filter.reset();
  decoder.reset();
  transcoder.reset();
  if (processorOptions.saveRecord) {
    decoder = std::make_unique<codec::Decoder>(decoderOptions);
    filter = std::make_unique<codec::Filter>(filterOptions);
    encoder = std::make_unique<codec::Encoder>(encoderOptions);
    transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  }
}

void AppStreamProcessorRunner::ResetWriter() {
  if (transcoder) {
    transcoder->Flush(*this);
  }
  writer.reset();
  if (processorOptions.saveRecord) {
    recorderStartTime = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&recorderStartTime));
    writer = std::make_unique<codec::Writer>(buf, writerOptions);
  }
}

void AppStreamProcessorRunner::operator()(const RecordingStart&) {
  if (not processorOptions.saveRecord) {
    processorOptions.saveRecord = true;
    Reset();
  }
}

void AppStreamProcessorRunner::operator()(const RecordingStop&) {
  processorOptions.saveRecord = false;
  Reset();
}

void AppStreamProcessorRunner::operator()(const ProcessBuffer& recordBuffer) {
  Process(recordBuffer.buffer);
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
