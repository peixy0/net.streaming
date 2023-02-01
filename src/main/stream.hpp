#pragma once

#include <deque>
#include <set>
#include <thread>
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "video.hpp"

namespace application {

class AppStreamTranscoder : public codec::EncodedDataProcessor {
public:
  AppStreamTranscoder(std::unique_ptr<codec::Decoder>, std::unique_ptr<codec::Filter>, std::unique_ptr<codec::Encoder>,
      std::unique_ptr<codec::Transcoder>, std::unique_ptr<codec::Writer>);
  ~AppStreamTranscoder() override;
  void Process(std::string_view);
  void ProcessEncodedData(AVPacket*) override;

private:
  std::unique_ptr<codec::Decoder> decoder;
  std::unique_ptr<codec::Filter> filter;
  std::unique_ptr<codec::Encoder> encoder;
  std::unique_ptr<codec::Transcoder> transcoder;
  std::unique_ptr<codec::Writer> writer;
};

class AppStreamTranscoderFactory {
public:
  AppStreamTranscoderFactory(const codec::DecoderOptions&, const codec::FilterOptions&, const codec::EncoderOptions&,
      const codec::WriterOptions&);
  std::unique_ptr<AppStreamTranscoder> Create(codec::WriterProcessor&) const;
  std::unique_ptr<AppStreamTranscoder> Create(std::string_view) const;

private:
  const codec::DecoderOptions decoderOptions;
  const codec::FilterOptions filterOptions;
  const codec::EncoderOptions encoderOptions;
  const codec::WriterOptions writerOptions;
};

struct AppStreamRecorderOptions {
  std::string format;
  bool saveRecord;
  std::uint32_t maxRecordingTimeInSeconds;
};

struct StartRecording {};
struct StopRecording {};
struct RecordData {
  std::string buffer;
};
using AppRecorderEvent = std::variant<StartRecording, StopRecording, RecordData>;

class AppStreamRecorderRunner {
public:
  AppStreamRecorderRunner(
      common::EventQueue<AppRecorderEvent>&, const AppStreamRecorderOptions&, AppStreamTranscoderFactory&);
  void Run();
  void Process(std::string_view);
  void operator()(const StartRecording&);
  void operator()(const StopRecording&);
  void operator()(const RecordData&);

private:
  void Reset();

  std::thread processorThread;
  common::EventQueue<AppRecorderEvent>& eventQueue;
  AppStreamRecorderOptions recorderOptions;
  AppStreamTranscoderFactory& transcoderFactory;

  std::unique_ptr<AppStreamTranscoder> transcoder;
  std::time_t recorderStartTime;
};

class AppStreamReceiver {
public:
  virtual ~AppStreamReceiver() = default;
  virtual void Notify(std::string_view) = 0;
};

class AppStreamDistributer {
public:
  void Process(std::string_view);
  void AddSubscriber(AppStreamReceiver*);
  void RemoveSubscriber(AppStreamReceiver*);

private:
  std::set<AppStreamReceiver*> receivers;
  mutable std::mutex receiversMut;
};

class AppStreamCapturerRunner : public video::StreamProcessor {
public:
  AppStreamCapturerRunner(const video::CapturerOptions&, AppStreamDistributer&);
  void Run();
  void ProcessFrame(std::string_view) override;

private:
  const video::CapturerOptions capturerOptions;
  std::thread capturerThread;
  AppStreamDistributer& streamDistributer;
};

}  // namespace application
