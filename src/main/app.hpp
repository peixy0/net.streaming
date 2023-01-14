#pragma once
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(
      AppStreamDistributer&, AppStreamDistributer&, AppStreamSnapshotSaver&, common::EventQueue<StreamProcessorEvent>&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppStreamDistributer& mjpegDistributer;
  AppStreamDistributer& h264Distributer;
  AppStreamSnapshotSaver& snapshotSaver;
  common::EventQueue<StreamProcessorEvent>& streamProcessorEventQueue;
  bool isRecording{false};
};

}  // namespace application
