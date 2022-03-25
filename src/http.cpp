#include "http.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include "network.hpp"

namespace {

std::string to_string(network::HttpStatus status) {
  switch (status) {
    case network::HttpStatus::OK:
      return "200 OK";
    case network::HttpStatus::NotFound:
      return "404 Not Found";
  }
  return "";
}

}  // namespace

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
  auto response = processor.Process(std::move(*request));
  std::string respPacket = "HTTP/1.1 " + to_string(response.status) + "\r\n";
  for (const auto& header : response.headers) {
    respPacket += header.first + ": " + header.second + "\r\n";
  }
  respPacket += "content-length: " + std::to_string(response.body.length()) + "\r\n\r\n";
  respPacket += response.body;
  sender.Send(respPacket);
}

HttpLayerFactory::HttpLayerFactory(HttpProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(NetworkSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(std::move(parser), processor, sender);
}

}  // namespace network
