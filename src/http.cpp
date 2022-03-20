#include "http.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"

namespace protocol {

HttpLayer::HttpLayer(const HttpOptions& options, network::NetworkSender& sender) : options{options}, sender{sender} {
}

void HttpLayer::Receive(std::string_view buf) {
  payloadSize += buf.size();
  if (payloadSize > options.maxPayloadSize) {
    receivedPayload.clear();
    sender.Close();
    return;
  }
  receivedPayload.append(buf.cbegin(), buf.cend());
  if (receivedPayload.ends_with("\r\n\r\n") or receivedPayload.ends_with("\n\n")) {
    spdlog::debug(receivedPayload);
    receivedPayload.clear();
    sender.Send("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO");
    sender.Close();
  }
}

HttpLayerFactory::HttpLayerFactory(const HttpOptions& options) : options{options} {
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(network::NetworkSender& sender) const {
  return std::make_unique<HttpLayer>(options, sender);
}

}  // namespace protocol
