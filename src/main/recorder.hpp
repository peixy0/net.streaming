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
  ~AppStreamRecorder();
  void ProcessBuffer(std::string_view);
  void ProcessEncodedData(AVPacket*) override;

private:
  codec::Transcoder& transcoder;
  codec::Writer& writer;
};

struct StartRecording {};
struct StopRecording {};
struct RecordBuffer {
  std::string buffer;
};
using RecorderEvent = std::variant<StartRecording, StopRecording, RecordBuffer>;

class AppStreamRecorderRunner {
public:
  AppStreamRecorderRunner(const codec::DecoderOptions&, const codec::FilterOptions&, const codec::EncoderOptions&,
                          const codec::WriterOptions&, common::EventQueue<RecorderEvent>&);
  void Run();
  void operator()(const StartRecording&);
  void operator()(const StopRecording&);
  void operator()(const RecordBuffer&);

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
  common::EventQueue<RecorderEvent>& eventQueue;
};

}  // namespace application
