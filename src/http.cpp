#include "http.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"

namespace network {

HttpLayer::HttpLayer(HttpProcessor& processor, std::unique_ptr<NetworkSender> sender)
    : processor{processor}, sender{std::move(sender)} {
}

void HttpLayer::Receive(std::string_view packet) {
  spdlog::debug(packet);
  HttpRequest request{"/"};
  auto response = processor.Process(request);
  sender->Send(
      "HTTP/1.1 200 OK\r\n"
      "content-length: " +
      std::to_string(response.body.length()) + "\r\n\r\n" + response.body);
  sender->Close();
}

HttpLayerFactory::HttpLayerFactory(HttpProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(std::unique_ptr<NetworkSender> sender) const {
  return std::make_unique<HttpLayer>(processor, std::move(sender));
}

}  // namespace network
