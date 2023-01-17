#pragma once

#include <deque>
#include <set>
#include <thread>
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "video.hpp"

namespace application {

class AppStreamDistributer;

struct AppStreamProcessorOptions {
  bool distributeMjpeg;
  bool saveRecord;
  int maxRecordingTimeInSeconds;
};

struct RecordingStart {};
struct RecordingStop {};
struct ProcessBuffer {
  std::string buffer;
};
using StreamProcessorEvent = std::variant<RecordingStart, RecordingStop, ProcessBuffer>;

class AppStreamProcessorRunner : public codec::EncodedDataProcessor {
public:
  AppStreamProcessorRunner(common::EventQueue<StreamProcessorEvent>&, application::AppStreamDistributer&,
      const AppStreamProcessorOptions&, const codec::DecoderOptions&, const codec::FilterOptions&,
      const codec::EncoderOptions&, const codec::WriterOptions&);
  void Run();
  void Process(std::string_view);
  void ProcessEncodedData(AVPacket*) override;
  void operator()(const RecordingStart&);
  void operator()(const RecordingStop&);
  void operator()(const ProcessBuffer&);

private:
  void Reset();
  void ResetWriter();

  std::thread processorThread;
  common::EventQueue<StreamProcessorEvent>& eventQueue;
  application::AppStreamDistributer& mjpegDistributer;

  AppStreamProcessorOptions processorOptions;
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

class AppStreamSnapshotSaver : public AppStreamReceiver {
public:
  explicit AppStreamSnapshotSaver(AppStreamDistributer&);
  ~AppStreamSnapshotSaver() override;
  void Notify(std::string_view) override;
  std::string GetSnapshot() const;

private:
  AppStreamDistributer& distributer;
  std::string snapshot;
  mutable std::mutex snapshotMut;
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
  AppStreamCapturerRunner(const video::StreamOptions&, common::EventQueue<StreamProcessorEvent>&);
  void Run();
  void ProcessFrame(std::string_view) override;

private:
  const video::StreamOptions streamOptions;
  std::thread streamThread;
  common::EventQueue<StreamProcessorEvent>& processorEventQueue;
};

}  // namespace application
