#include "app.hpp"
#include <utmp.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <thread>

namespace application {
AppLayer::AppLayer() {
  StartDaemon();
}

AppLayer::~AppLayer() {
  running = false;
  daemon.join();
}

network::HttpResponse AppLayer::Process(const network::HttpRequest&) {
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
    auto s = std::make_shared<std::string>();
    utmpname("/var/log/btmp");
    setutent();
    for (int i = 0; i < 20; i++) {
      auto utmp = getutent();
      if (not utmp) {
        break;
      }
      s->append(utmp->ut_user);
      s->append(" ");
      s->append(utmp->ut_host);
      s->append(" ");
      s->append(utmp->ut_line);
      s->append(" ");
      time_t t = utmp->ut_tv.tv_sec;
      char buf[50];
      std::strftime(buf, sizeof buf, "%c %Z", std::gmtime(&t));
      s->append(buf);
      s->append("\n");
    }
    endutent();
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
