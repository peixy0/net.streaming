#pragma once
#include <shared_mutex>
#include <thread>
#include "network.hpp"

namespace application {

class AppLayer : public network::HttpProcessor {
public:
  AppLayer();
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  void StartDaemon();
  void DaemonTask();

  std::atomic<bool> running = true;
  std::thread daemon;
  std::shared_mutex mutex;
  std::shared_ptr<std::string> content = std::make_shared<std::string>("");
};

}  // namespace application
