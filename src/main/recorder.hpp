#pragma once

#include <string_view>
#include <thread>
#include <variant>
#include "codec.hpp"
#include "event_queue.hpp"

namespace application {

class AppStreamRecorder : public codec::EncodedDataProcessor {
public:
  AppStreamRecorder(codec::Transcoder&, codec::Writer&);
  ~AppStreamRecorder() override;
  void Record(std::string_view);
  void ProcessEncodedData(AVPacket*) override;

private:
  codec::Transcoder& transcoder;
  codec::Writer& writer;
};

struct RecordingStart {};
struct RecordingStop {};
struct RecordingAppend {
  std::string buffer;
};
using RecordingEvent = std::variant<RecordingStart, RecordingStop, RecordingAppend>;

class AppStreamRecorderRunner {
public:
  AppStreamRecorderRunner(const codec::DecoderOptions&, const codec::FilterOptions&, const codec::EncoderOptions&,
                          const codec::WriterOptions&, common::EventQueue<RecordingEvent>&);
  void Run();
  void operator()(const RecordingStart&);
  void operator()(const RecordingStop&);
  void operator()(const RecordingAppend&);

private:
  codec::DecoderOptions decoderOptions;
  codec::FilterOptions filterOptions;
  codec::EncoderOptions encoderOptions;
  codec::WriterOptions writerOptions;

  std::unique_ptr<codec::Decoder> decoder;
  std::unique_ptr<codec::Filter> filter;
  std::unique_ptr<codec::Encoder> encoder;
  std::unique_ptr<codec::Transcoder> transcoder;
  std::unique_ptr<codec::Writer> writer;
  std::unique_ptr<AppStreamRecorder> recorder;

  std::thread recorderThread;
  common::EventQueue<RecordingEvent>& eventQueue;
};

}  // namespace application
