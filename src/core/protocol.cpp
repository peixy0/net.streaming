#include "protocol.hpp"
#include <spdlog/spdlog.h>

namespace network {

ProtocolLayer::ProtocolLayer(TcpSender& sender, RouterFactory& routerFactory)
    : httpSender{sender},
      websocketSender{sender},
      router{routerFactory.Create(httpSender, websocketSender, *this)},
      processor{std::make_unique<HttpLayer>(httpParser, httpSender, *router)} {
}

void ProtocolLayer::Process(std::string_view payload) {
  buffer += payload;
  while (processor->TryProcess(buffer)) {
  }
}

void ProtocolLayer::UpgradeToWebsocket() {
  processor = std::make_unique<WebsocketLayer>(websocketParser, websocketSender, *router);
}

ProtocolLayerFactory::ProtocolLayerFactory(RouterFactory& routerFactory) : routerFactory{routerFactory} {
}

std::unique_ptr<TcpProcessor> ProtocolLayerFactory::Create(TcpSender& sender) const {
  return std::make_unique<ProtocolLayer>(sender, routerFactory);
}

}  // namespace network
