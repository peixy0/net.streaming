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
  bool distributeH264;
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
      application::AppStreamDistributer&, const AppStreamProcessorOptions&, const codec::DecoderOptions&,
      const codec::FilterOptions&, const codec::EncoderOptions&, const codec::WriterOptions&);
  void Run();
  void StartRecording();
  void StopRecording();
  void StartTranscoding();
  void StopTranscoding();
  void Process(std::string_view);
  void ProcessEncodedData(AVPacket*) override;
  void operator()(const RecordingStart&);
  void operator()(const RecordingStop&);
  void operator()(const ProcessBuffer&);

private:
  void Reset();

  std::thread processorThread;
  common::EventQueue<StreamProcessorEvent>& eventQueue;
  application::AppStreamDistributer& mjpegDistributer;
  application::AppStreamDistributer& h264Distributer;

  AppStreamProcessorOptions processorOptions;
  codec::DecoderOptions decoderOptions;
  codec::FilterOptions filterOptions;
  codec::EncoderOptions encoderOptions;
  codec::WriterOptions writerOptions;

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

class AppStream : public AppStreamReceiver, public network::RawStream {
public:
  AppStream(AppStreamDistributer&, network::SenderNotifier&);
  virtual ~AppStream() = default;
  std::optional<std::string> GetBuffered() override;

protected:
  AppStreamDistributer& distributer;
  network::SenderNotifier& notifier;

  std::deque<std::string> streamBuffer;
  std::mutex bufferMut;
};

class AppMjpegStream : public AppStream {
public:
  AppMjpegStream(AppStreamDistributer&, network::SenderNotifier&);
  ~AppMjpegStream() override;
  void Notify(std::string_view) override;

private:
  int skipped{0};
};

class AppH264Stream : public AppStream {
public:
  AppH264Stream(AppStreamDistributer&, network::SenderNotifier&);
  ~AppH264Stream() override;
  void Notify(std::string_view) override;
};

class AppMjpegStreamFactory : public network::RawStreamFactory {
public:
  explicit AppMjpegStreamFactory(AppStreamDistributer&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&) override;

private:
  AppStreamDistributer& distributer;
};

class AppH264StreamFactory : public network::RawStreamFactory {
public:
  explicit AppH264StreamFactory(AppStreamDistributer&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&) override;

private:
  AppStreamDistributer& distributer;
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
