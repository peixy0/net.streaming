#include "app.hpp"
#include <spdlog/spdlog.h>
#include <utmp.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <thread>
#include <vector>
#include "network.hpp"

namespace application {

AppLayer::AppLayer() {
  StartBtmpLoaderDaemon();
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  for (const auto& [field, value] : req.headers) {
    spdlog::debug("app received header {}: {}", field, value);
  }
  if (req.uri == "/lastb") {
    std::shared_lock l{btmpMutex};
    return network::PlainTextHttpResponse{network::HttpStatus::OK, *btmpContent};
  }
  if (req.uri == "/nginx.access") {
    return network::FileHttpResponse{"/var/log/nginx/access.log", "text/plain;charset=utf-8"};
  }
  return network::PlainTextHttpResponse{network::HttpStatus::NotFound, ""};
}

void AppLayer::StartBtmpLoaderDaemon() {
  daemon = std::thread{[this] { LoadBtmpContent(); }};
}

void AppLayer::LoadBtmpContent() {
  while (true) {
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

    std::string result;
    int n = 0;
    for (auto it = entries.rbegin(); it != entries.rend() and n < 50; it++) {
      time_t t = it->ut_tv.tv_sec;
      char timebuf[50];
      std::strftime(timebuf, sizeof timebuf, "%c %Z", std::gmtime(&t));
      result += timebuf;
      result += " ";
      result += it->ut_line;
      result += " ";
      result += it->ut_host;
      result += "\t";
      result += it->ut_user;
      result += "\n";
      n++;
    }

    auto s = std::make_shared<std::string>(result);
    {
      std::unique_lock l{btmpMutex};
      btmpContent = s;
    }
    {
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(10min);
    }
  }
}

}  // namespace application
