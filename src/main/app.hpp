#pragma once
#include <mutex>
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppMjpegSender : public AppStreamReceiver, public network::HttpProcessor {
public:
  AppMjpegSender(AppStreamDistributer&, network::HttpSender&, int skipCount = 0);
  ~AppMjpegSender() override;
  void Notify(std::string_view) override;
  void Process(network::HttpRequest&&) override;

private:
  AppStreamDistributer& mjpegDistributer;
  network::HttpSender& sender;
  int skipped{0};
  const int skipCount;
};

class AppMjpegSenderFactory : public network::HttpProcessorFactory {
public:
  explicit AppMjpegSenderFactory(AppStreamDistributer&, int skipCount = 0);
  std::unique_ptr<network::HttpProcessor> Create(network::HttpSender&) const override;

private:
  AppStreamDistributer& distributer;
  const int skipCount;
};

class AppEncodedStreamSender : public AppStreamReceiver, public codec::WriterProcessor, public network::HttpProcessor {
public:
  AppEncodedStreamSender(AppStreamDistributer&, AppStreamTranscoderFactory&, network::HttpSender&);
  ~AppEncodedStreamSender() override;
  void Notify(std::string_view) override;
  void WriteData(std::string_view) override;
  void Process(network::HttpRequest&&) override;

private:
  void RunTranscoder();

  AppStreamDistributer& mjpegDistributer;
  std::unique_ptr<AppStreamTranscoder> transcoder;
  network::HttpSender& sender;
  common::ConcreteEventQueue<std::optional<std::string>> transcoderQueue;
  std::thread transcoderThread;
};

class AppEncodedStreamSenderFactory : public network::HttpProcessorFactory {
public:
  AppEncodedStreamSenderFactory(AppStreamDistributer&, AppStreamTranscoderFactory&);
  std::unique_ptr<network::HttpProcessor> Create(network::HttpSender&) const override;

private:
  AppStreamDistributer& distributer;
  AppStreamTranscoderFactory& transcoderFactory;
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

class AppStreamRecorderController : public AppStreamReceiver {
public:
  explicit AppStreamRecorderController(AppStreamDistributer&, common::EventQueue<AppRecorderEvent>&);
  ~AppStreamRecorderController() override;
  void Notify(std::string_view) override;
  void Start();
  void Stop();
  bool IsRecording() const;

private:
  AppStreamDistributer& streamDistributer;
  common::EventQueue<AppRecorderEvent>& eventQueue;
  bool isRecording{false};
  mutable std::mutex confMut;
};

class AppHttpLayer {
public:
  AppHttpLayer(AppStreamSnapshotSaver&, AppStreamRecorderController&);
  AppHttpLayer(const AppHttpLayer&) = delete;
  AppHttpLayer(AppHttpLayer&&) = delete;
  AppHttpLayer& operator=(const AppHttpLayer&) = delete;
  AppHttpLayer& operator=(AppHttpLayer&&) = delete;
  ~AppHttpLayer() = default;

  void GetIndex(network::HttpRequest&&, network::HttpSender&) const;
  void GetSnapshot(network::HttpRequest&&, network::HttpSender&) const;
  void GetRecording(network::HttpRequest&&, network::HttpSender&) const;
  void SetRecording(network::HttpRequest&&, network::HttpSender&) const;

private:
  AppStreamSnapshotSaver& snapshotSaver;
  AppStreamRecorderController& processorController;
};

}  // namespace application
