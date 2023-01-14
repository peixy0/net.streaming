#include "recorder.hpp"
#include <ctime>

namespace application {

AppStreamRecorder::AppStreamRecorder(codec::Transcoder& transcoder, codec::Writer& writer)
    : transcoder{transcoder}, writer{writer} {
}

AppStreamRecorder::~AppStreamRecorder() {
  transcoder.Flush(*this);
}

void AppStreamRecorder::Record(std::string_view buffer) {
  transcoder.Process(buffer, *this);
}

void AppStreamRecorder::ProcessEncodedData(AVPacket* packet) {
  writer.Process(packet);
}

AppStreamRecorderRunner::AppStreamRecorderRunner(common::EventQueue<RecordingEvent>& eventQueue,
                                                 const RecorderOptions& recorderOptions,
                                                 const codec::DecoderOptions& decoderOptions,
                                                 const codec::FilterOptions& filterOptions,
                                                 const codec::EncoderOptions& encoderOptions,
                                                 const codec::WriterOptions& writerOptions)
    : eventQueue{eventQueue},
      recorderOptions{recorderOptions},
      decoderOptions{decoderOptions},
      filterOptions{filterOptions},
      encoderOptions{encoderOptions},
      writerOptions{writerOptions} {
}

void AppStreamRecorderRunner::Run() {
  recorderThread = std::thread([this] {
    while (true) {
      auto event = eventQueue.Pop();
      std::visit(*this, event);
    }
  });
}

void AppStreamRecorderRunner::StartRecording() {
  StopRecording();
  startTime = std::time(nullptr);
  char buf[50];
  std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&startTime));
  decoder = std::make_unique<codec::Decoder>(decoderOptions);
  filter = std::make_unique<codec::Filter>(filterOptions);
  encoder = std::make_unique<codec::Encoder>(encoderOptions);
  transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  writer = std::make_unique<codec::Writer>(buf, writerOptions);
  recorder = std::make_unique<AppStreamRecorder>(*transcoder, *writer);
}

void AppStreamRecorderRunner::StopRecording() {
  recorder.reset();
  writer.reset();
  transcoder.reset();
  encoder.reset();
  filter.reset();
  decoder.reset();
}

void AppStreamRecorderRunner::Record(std::string_view buffer) {
  if (not recorder) {
    return;
  }
  if (recorderOptions.maxRecordingTimeInSeconds > 0) {
    const auto now = std::time(nullptr);
    const auto diff = std::difftime(now, startTime);
    if (diff >= recorderOptions.maxRecordingTimeInSeconds) {
      StartRecording();
    }
  }
  recorder->Record(buffer);
}

void AppStreamRecorderRunner::operator()(const RecordingStart&) {
  StartRecording();
}

void AppStreamRecorderRunner::operator()(const RecordingStop&) {
  StopRecording();
}

void AppStreamRecorderRunner::operator()(const RecordingAppend& recordBuffer) {
  Record(recordBuffer.buffer);
}

}  // namespace application
