#pragma once
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(AppStreamDistributer&, AppStreamSnapshotSaver&, const AppStreamProcessorOptions&,
      common::EventQueue<StreamProcessorEvent>&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppMjpegLowFramerateStreamFactory mjpegLowFramerateStreamFactory;
  AppMjpegHighFramerateStreamFactory mjpegHighFramerateStreamFactory;
  AppStreamSnapshotSaver& snapshotSaver;

  AppStreamProcessorOptions streamProcessorOptions;
  common::EventQueue<StreamProcessorEvent>& streamProcessorEventQueue;
};

}  // namespace application
