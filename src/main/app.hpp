#pragma once
#include "event_queue.hpp"
#include "network.hpp"
#include "stream.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(AppStreamDistributer&, AppStreamDistributer&, AppStreamSnapshotSaver&, const AppStreamProcessorOptions&,
      common::EventQueue<StreamProcessorEvent>&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppMjpegStreamFactory mjpegStreamFactory;
  AppH264StreamFactory h264StreamFactory;
  AppStreamSnapshotSaver& snapshotSaver;

  AppStreamProcessorOptions streamProcessorOptions;
  common::EventQueue<StreamProcessorEvent>& streamProcessorEventQueue;
};

}  // namespace application
