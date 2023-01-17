#pragma once

#include <deque>
#include <set>
#include <thread>
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "video.hpp"

namespace application {

struct AppStreamRecorderOptions {
  bool saveRecord;
  int maxRecordingTimeInSeconds;
};

struct StartRecording {};
struct StopRecording {};
struct RecordData {
  std::string buffer;
};
using AppRecorderEvent = std::variant<StartRecording, StopRecording, RecordData>;

class AppStreamRecorderRunner : public codec::EncodedDataProcessor {
public:
  AppStreamRecorderRunner(common::EventQueue<AppRecorderEvent>&, const AppStreamRecorderOptions&,
      const codec::DecoderOptions&, const codec::FilterOptions&, const codec::EncoderOptions&,
      const codec::WriterOptions&);
  void Run();
  void Process(std::string_view);
  void ProcessEncodedData(AVPacket*) override;
  void operator()(const StartRecording&);
  void operator()(const StopRecording&);
  void operator()(const RecordData&);

private:
  void Reset();
  void ResetWriter();

  std::thread processorThread;
  common::EventQueue<AppRecorderEvent>& eventQueue;

  AppStreamRecorderOptions recorderOptions;
  const codec::DecoderOptions decoderOptions;
  const codec::FilterOptions filterOptions;
  const codec::EncoderOptions encoderOptions;
  const codec::WriterOptions writerOptions;

  std::time_t recorderStartTime;
  std::unique_ptr<codec::Decoder> decoder;
  std::unique_ptr<codec::Filter> filter;
  std::unique_ptr<codec::Encoder> encoder;
  std::unique_ptr<codec::Transcoder> transcoder;
  std::unique_ptr<codec::Writer> writer;
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
  std::mutex mutable receiversMut;
};

class AppStreamCapturerRunner : public video::StreamProcessor {
public:
  AppStreamCapturerRunner(const video::StreamOptions&, AppStreamDistributer&);
  void Run();
  void ProcessFrame(std::string_view) override;

private:
  const video::StreamOptions streamOptions;
  std::thread streamThread;
  AppStreamDistributer& streamDistributer;
};

}  // namespace application
