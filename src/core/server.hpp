#pragma once
#include <string>
#include "network.hpp"
#include "router.hpp"

namespace network {

class Server {
public:
  void Start(std::string_view, std::uint16_t);
  void Add(HttpMethod, const std::string&, std::unique_ptr<HttpProcessorFactory>);
  void Add(HttpMethod, const std::string&, std::function<void(HttpRequest&&, HttpSender&)>);
  void Add(const std::string&, std::unique_ptr<WebsocketProcessorFactory>);
  void Add(const std::string&, std::function<void(WebsocketFrame&&, WebsocketSender&)>);

private:
  HttpRouteMapping httpMapping;
  WebsocketRouteMapping websocketMapping;
};

}  // namespace network
