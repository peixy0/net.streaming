#pragma once
#include <mutex>
#include "codec.hpp"
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppLowFrameRateMjpegSender : public AppStreamReceiver {
public:
  AppLowFrameRateMjpegSender(network::HttpSender&, AppStreamDistributer&);
  ~AppLowFrameRateMjpegSender() override;
  void Notify(std::string_view) override;

private:
  network::HttpSender& sender;
  AppStreamDistributer& mjpegDistributer;
  int skipped{0};
};

class AppHighFrameRateMjpegSender : public AppStreamReceiver {
public:
  AppHighFrameRateMjpegSender(network::HttpSender&, AppStreamDistributer&);
  ~AppHighFrameRateMjpegSender() override;
  void Notify(std::string_view) override;

private:
  network::HttpSender& sender;
  AppStreamDistributer& mjpegDistributer;
};

class AppEncodedStreamSender : public AppStreamReceiver, public codec::WriterProcessor {
public:
  AppEncodedStreamSender(network::HttpSender&, AppStreamDistributer&, AppStreamTranscoderFactory&);
  ~AppEncodedStreamSender() override;
  void Notify(std::string_view) override;
  void WriteData(std::string_view) override;
  void RunTranscoder();
  void RunSender();

private:
  common::ConcreteEventQueue<std::optional<std::string>> transcoderQueue;
  common::ConcreteEventQueue<std::optional<std::string>> senderQueue;
  std::thread transcoderThread;
  std::thread senderThread;
  network::HttpSender& sender;
  AppStreamDistributer& mjpegDistributer;
  std::unique_ptr<AppStreamTranscoder> transcoder;
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

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(network::HttpSender&, AppStreamDistributer&, AppStreamSnapshotSaver&, AppStreamRecorderController&,
      AppStreamTranscoderFactory&);
  AppLayer(const AppLayer&) = delete;
  AppLayer(AppLayer&&) = delete;
  AppLayer& operator=(const AppLayer&) = delete;
  AppLayer& operator=(AppLayer&&) = delete;
  ~AppLayer() override;

  void Process(network::HttpRequest&&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  network::HttpSender& sender;
  AppStreamDistributer& mjpegDistributer;
  AppStreamSnapshotSaver& snapshotSaver;
  AppStreamRecorderController& processorController;
  AppStreamTranscoderFactory& transcoderFactory;
  std::unique_ptr<AppStreamReceiver> streamSender;
};

class AppLayerFactory : public network::HttpProcessorFactory {
public:
  AppLayerFactory(
      AppStreamDistributer&, AppStreamSnapshotSaver&, AppStreamRecorderController&, AppStreamTranscoderFactory&);
  std::unique_ptr<network::HttpProcessor> Create(network::HttpSender&, network::ProtocolUpgrader&) const override;

private:
  AppStreamDistributer& mjpegDistributer;
  AppStreamSnapshotSaver& snapshotSaver;
  AppStreamRecorderController& recorderController;
  AppStreamTranscoderFactory& transcoderFactory;
};

}  // namespace application
