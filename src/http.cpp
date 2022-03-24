#include "http.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include "network.hpp"

namespace network {

HttpLayer::HttpLayer(std::unique_ptr<HttpParser> parser, HttpProcessor& processor, NetworkSender& sender)
    : parser{std::move(parser)}, processor{processor}, sender{sender} {
}

void HttpLayer::Receive(std::string_view packet) {
  payloadSize += packet.size();
  if (payloadSize > 1 << 20) {
    spdlog::error("http received payload exceeds limit");
    sender.Close();
    return;
  }
  spdlog::debug("http received packet: {}", packet);
  receivedPayload.append(packet);
  auto request = parser->Parse(receivedPayload);
  if (not request) {
    return;
  }
  payloadSize = receivedPayload.size();
  if (request->uri != "/") {
    sender.Send("HTTP/1.1 404 Not Found\r\n\r\n");
    return;
  }
  auto response = processor.Process(*request);
  sender.Send(
      "HTTP/1.1 200 OK\r\n"
      "content-length: " +
      std::to_string(response.body.length()) + "\r\n\r\n" + response.body);
}

HttpLayerFactory::HttpLayerFactory(HttpProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(NetworkSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(std::move(parser), processor, sender);
}

}  // namespace network
