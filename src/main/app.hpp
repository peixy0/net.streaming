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
  void StartBtmpLoaderDaemon();
  void LoadBtmpContent();

  std::atomic<bool> running = true;
  std::thread daemon;
  std::shared_mutex btmpMutex;
  std::shared_ptr<std::string> btmpContent = std::make_shared<std::string>("");
};

}  // namespace application
