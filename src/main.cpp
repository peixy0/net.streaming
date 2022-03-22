#include <spdlog/spdlog.h>
#include "app.hpp"
#include "http.hpp"
#include "tcp.hpp"

int main() {
  spdlog::set_level(spdlog::level::info);
  application::AppLayer app;
  network::HttpLayerFactory factory{app};
  network::Tcp4Layer tcp{"0.0.0.0", 8080, factory};
  tcp.Start();
  return 0;
}
