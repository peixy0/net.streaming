#include "recorder.hpp"
#include <ctime>

namespace application {

AppStreamRecorderRunner::AppStreamRecorderRunner(const codec::DecoderOptions& decoderOptions,
                                                 const codec::FilterOptions& filterOptions,
                                                 const codec::EncoderOptions& encoderOptions,
                                                 const codec::WriterOptions& writerOptions,
                                                 common::EventQueue<RecorderEvent>& eventQueue)
    : decoderOptions{decoderOptions},
      filterOptions{filterOptions},
      encoderOptions{encoderOptions},
      writerOptions{writerOptions},
      eventQueue{eventQueue} {
}

void AppStreamRecorderRunner::Run() {
  recorderThread = std::thread([this] {
    while (true) {
      auto event = eventQueue.Pop();
      std::visit(*this, event);
    }
  });
}

void AppStreamRecorderRunner::operator()(const StartRecording&) {
  std::time_t tm = std::time(nullptr);
  char buf[50];
  std::strftime(buf, sizeof buf, "%Y.%m.%d.%H.%M.%S.mp4", std::localtime(&tm));
  decoder = std::make_unique<codec::Decoder>(decoderOptions);
  filter = std::make_unique<codec::Filter>(filterOptions);
  encoder = std::make_unique<codec::Encoder>(encoderOptions);
  transcoder = std::make_unique<codec::Transcoder>(*decoder, *filter, *encoder);
  writer = std::make_unique<codec::Writer>(buf, writerOptions);
  recorder = std::make_unique<AppStreamRecorder>(*transcoder, *writer);
}

void AppStreamRecorderRunner::operator()(const StopRecording&) {
  recorder.reset();
  writer.reset();
  transcoder.reset();
  encoder.reset();
  filter.reset();
  decoder.reset();
}

void AppStreamRecorderRunner::operator()(const RecordBuffer& recordBuffer) {
  if (recorder) {
    recorder->ProcessBuffer(recordBuffer.buffer);
  }
}

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

}  // namespace application
