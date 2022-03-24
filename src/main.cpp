#include <spdlog/spdlog.h>
#include <cstring>
#include "app.hpp"
#include "http.hpp"
#include "tcp.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    return -1;
  }
  std::uint16_t port = std::atoi(argv[1]);
  spdlog::set_level(spdlog::level::info);
  application::AppLayer app;
  network::HttpLayerFactory factory{app};
  network::Tcp4Layer tcp{"0.0.0.0", port, factory};
  tcp.Start();
  return 0;
}
