#include "http.hpp"
#include "network.hpp"

int main() {
  protocol::HttpOptions options;
  options.maxPayloadSize = 1 << 20;
  protocol::HttpLayerFactory factory{options};
  network::Tcp4Layer network{"0.0.0.0", 8080, factory};
  network.Start();
  return 0;
}
