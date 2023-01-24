#include "protocol.hpp"
#include <spdlog/spdlog.h>

namespace network {

ProtocolLayer::ProtocolLayer(TcpSender& sender_, HttpLayerFactory& httpLayerFactory)
    : sender{sender_}, httpLayerFactory{httpLayerFactory}, processor{httpLayerFactory.Create(sender, *this)} {
}

void ProtocolLayer::Process(std::string_view payload) {
  buffer += payload;
  while (true) {
    if (not processor) {
      break;
    }
    auto bufferLen = buffer.size();
    processor->Process(buffer);
    if (bufferLen == buffer.size()) {
      break;
    }
  }
}

void ProtocolLayer::Add(WebsocketLayerFactory* websocketLayerFactory_) {
  websocketLayerFactory = websocketLayerFactory_;
}

void ProtocolLayer::UpgradeToWebsocket() {
  processor.reset();
  if (websocketLayerFactory) {
    processor = websocketLayerFactory->Create(sender);
  }
}

ProtocolLayerFactory::ProtocolLayerFactory(std::unique_ptr<HttpLayerFactory> httpLayerFactory)
    : httpLayerFactory{std::move(httpLayerFactory)} {
}

std::unique_ptr<TcpProcessor> ProtocolLayerFactory::Create(TcpSender& sender) const {
  auto protocolLayer = std::make_unique<ProtocolLayer>(sender, *httpLayerFactory);
  protocolLayer->Add(websocketLayerFactory.get());
  return protocolLayer;
}

void ProtocolLayerFactory::Add(std::unique_ptr<WebsocketLayerFactory> websocketLayerFactory_) {
  websocketLayerFactory = std::move(websocketLayerFactory_);
}

}  // namespace network
