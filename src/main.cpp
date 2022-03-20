#include "http.hpp"
#include "network.hpp"

int main() {
  protocol::HttpLayerFactory factory;
  network::TcpLayer network{"0.0.0.0", 8080, factory};
  network.Start();
  return 0;
}
