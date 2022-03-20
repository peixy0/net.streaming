#include "http.hpp"
#include "network.hpp"

namespace protocol {

HttpLayer::HttpLayer(network::NetworkSender& sender) : sender{sender} {
}

void HttpLayer::Receive(std::string_view buf) {
  payloadSize += buf.size();
  if (payloadSize > (1 << 20)) {
    receivedPayload.clear();
    sender.Close();
    return;
  }
  receivedPayload.append(buf.cbegin(), buf.cend());
  if (receivedPayload.ends_with("\r\n\r\n") or receivedPayload.ends_with("\n\n")) {
    printf("%s", receivedPayload.c_str());
    receivedPayload.clear();
    sender.Send("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO");
    sender.Close();
  }
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(network::NetworkSender& sender) const {
  return std::make_unique<HttpLayer>(sender);
}

}  // namespace protocol
