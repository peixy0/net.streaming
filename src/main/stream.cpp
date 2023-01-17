#include "stream.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppStreamRecorderRunner::AppStreamRecorderRunner(common::EventQueue<AppRecorderEvent>& eventQueue,
    const AppStreamRecorderOptions& recorderOptions, const codec::DecoderOptions& decoderOptions,
    const codec::FilterOptions& filterOptions, const codec::EncoderOptions& encoderOptions,
    const codec::WriterOptions& writerOptions)
    : eventQueue{eventQueue},
      recorderOptions{recorderOptions},
      decoderOptions{decoderOptions},
      filterOptions{filterOptions},
      encoderOptions{encoderOptions},
      writerOptions{writerOptions} {
  Reset();
}

void AppStreamRecorderRunner::Run() {
  processorThread = std::thread([this] {
    while (true) {
      auto event = eventQueue.Pop();
      std::visit(*this, event);
    }
  });
}

void AppStreamRecorderRunner::Process(std::string_view buffer) {
  if (not recorderOptions.saveRecord) {
    return;
  }
  if (recorderOptions.maxRecordingTimeInSeconds > 0) {
    const auto now = std::time(nullptr);
    const auto diff = std::difftime(now, recorderStartTime);
    if (diff >= recorderOptions.maxRecordingTimeInSeconds) {
      Reset();
    }
  }
  transcoder->Process(buffer, *this);
}

void AppStreamRecorderRunner::ProcessEncodedData(AVPacket* encoded) {
  if (recorderOptions.saveRecord) {
    writer->Process(encoded);
  }
}

void AppStreamRecorderRunner::Reset() {
  ResetWriter();
  encoder.reset();
  filter.reset();
  decoder.reset();
  transcoder.reset();
  if (recorderOptions.saveRecord) {
    decoder = std::make_unique<codec::Decoder>(decoderOptions);
    filter = std::make_unique<codec::Filter>(filterOptions);
    encoder = std::make_unique<codec::Encoder>(encoderOptions);
    transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  }
}

void AppStreamRecorderRunner::ResetWriter() {
  if (transcoder) {
    transcoder->Flush(*this);
  }
  writer.reset();
  if (recorderOptions.saveRecord) {
    recorderStartTime = std::time(nullptr);
    char buf[50];
    std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&recorderStartTime));
    writer = std::make_unique<codec::Writer>(buf, writerOptions);
  }
}

void AppStreamRecorderRunner::operator()(const StartRecording&) {
  if (not recorderOptions.saveRecord) {
    recorderOptions.saveRecord = true;
    Reset();
  }
}

void AppStreamRecorderRunner::operator()(const StopRecording&) {
  recorderOptions.saveRecord = false;
  Reset();
}

void AppStreamRecorderRunner::operator()(const RecordData& data) {
  Process(data.buffer);
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
    const video::StreamOptions& streamOptions, AppStreamDistributer& streamDistributer)
    : streamOptions{streamOptions}, streamDistributer{streamDistributer} {
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
  streamDistributer.Process({frame.data(), frame.size()});
}

}  // namespace application
