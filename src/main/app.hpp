#pragma once
#include <mutex>
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppLowFrameRateMjpegSender : public AppStreamReceiver {
public:
  AppLowFrameRateMjpegSender(AppStreamDistributer&, network::HttpSender&);
  ~AppLowFrameRateMjpegSender() override;
  void Notify(std::string_view) override;

private:
  AppStreamDistributer& mjpegDistributer;
  network::HttpSender& sender;
  int skipped{0};
};

class AppHighFrameRateMjpegSender : public AppStreamReceiver {
public:
  AppHighFrameRateMjpegSender(AppStreamDistributer&, network::HttpSender&);
  ~AppHighFrameRateMjpegSender() override;
  void Notify(std::string_view) override;

private:
  AppStreamDistributer& mjpegDistributer;
  network::HttpSender& sender;
};

class AppStreamProcessorController {
public:
  explicit AppStreamProcessorController(common::EventQueue<StreamProcessorEvent>&);
  void SetRecording(bool);
  bool IsRecording() const;

private:
  bool isRecording{false};
  common::EventQueue<StreamProcessorEvent>& eventQueue;
  mutable std::mutex confMut;
};

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(network::HttpSender&, AppStreamDistributer&, AppStreamSnapshotSaver&, AppStreamProcessorController&);
  AppLayer(const AppLayer&) = delete;
  AppLayer(AppLayer&&) = delete;
  AppLayer& operator=(const AppLayer&) = delete;
  AppLayer& operator=(AppLayer&&) = delete;
  ~AppLayer() override;

  void Process(network::HttpRequest&&) override;

private:
  network::PreparedHttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  network::HttpSender& sender;
  AppStreamDistributer& mjpegDistributer;
  AppStreamSnapshotSaver& snapshotSaver;
  AppStreamProcessorController& processorController;
  std::unique_ptr<AppStreamReceiver> streamReceiver;
};

class AppLayerFactory : public network::HttpProcessorFactory {
public:
  AppLayerFactory(AppStreamDistributer&, AppStreamSnapshotSaver&, AppStreamProcessorController&);
  std::unique_ptr<network::HttpProcessor> Create(network::HttpSender&) const override;

private:
  AppStreamDistributer& mjpegDistributer;
  AppStreamSnapshotSaver& snapshotSaver;
  AppStreamProcessorController& processorController;
};

}  // namespace application
