#pragma once
#include "event_queue.hpp"
#include "network.hpp"
#include "recorder.hpp"
#include "stream.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(AppStreamDistributer&, common::EventQueue<RecorderEvent>&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppStreamDistributer& streamDistributer;
  common::EventQueue<RecorderEvent>& recorderEventQueue;
  bool isRecording{false};
};

}  // namespace application
