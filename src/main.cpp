#include <spdlog/spdlog.h>
#include "http.hpp"
#include "network.hpp"

int main() {
  spdlog::set_level(spdlog::level::debug);
  protocol::HttpOptions options;
  options.maxPayloadSize = 1 << 20;
  protocol::HttpLayerFactory factory{options};
  network::Tcp4Layer network{"0.0.0.0", 8080, factory};
  network.Start();
  return 0;
}
