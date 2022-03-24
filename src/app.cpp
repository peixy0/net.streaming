#include "app.hpp"
#include <spdlog/spdlog.h>
#include <utmp.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <thread>
#include <vector>

namespace application {

AppLayer::AppLayer() {
  StartDaemon();
}

AppLayer::~AppLayer() {
  running = false;
  daemon.join();
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("http request {} {} {}", req.method, req.uri, req.version);
  for (const auto& [field, value] : req.headers) {
    spdlog::debug("http header {}: {}", field, value);
  }
  std::shared_lock l{mutex};
  return {*content};
}

void AppLayer::StartDaemon() {
  daemon = std::thread{[this] { DaemonTask(); }};
}

void AppLayer::DaemonTask() {
  while (true) {
    if (not running) {
      return;
    }
    std::vector<utmp> entries;
    utmpname("/var/log/btmp");
    setutent();
    for (;;) {
      auto ent = getutent();
      if (not ent) {
        break;
      }
      entries.emplace_back(*ent);
    }
    endutent();

    int n = 0;
    auto s = std::make_shared<std::string>();
    for (auto it = entries.rbegin(); it != entries.rend() and n < 50; it++) {
      time_t t = it->ut_tv.tv_sec;
      char buf[50];
      std::strftime(buf, sizeof buf, "%c %Z", std::gmtime(&t));
      s->append(buf);
      s->append(" ");
      s->append(it->ut_line);
      s->append(" ");
      s->append(it->ut_host);
      s->append("\t");
      s->append(it->ut_user);
      s->append("\n");
      n++;
    }
    {
      std::unique_lock l{mutex};
      content = s;
    }
    {
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(10min);
    }
  }
}

}  // namespace application
