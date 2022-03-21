#include <spdlog/spdlog.h>
#include "http.hpp"
#include "tcp.hpp"

int main() {
  spdlog::set_level(spdlog::level::info);
  network::HttpOptions options;
  options.maxPayloadSize = 1 << 20;
  network::HttpLayerFactory factory{options};
  network::TcpLayer tcp{"0.0.0.0", 8080, factory};
  tcp.Start();
  return 0;
}
