#pragma once
#include <memory>
#include "http.hpp"
#include "network.hpp"
#include "websocket.hpp"

namespace network {

class ProtocolLayer : public TcpProcessor, public ProtocolUpgrader {
public:
  ProtocolLayer(TcpSender&, RouterFactory&);
  ProtocolLayer(const ProtocolLayer&) = delete;
  ProtocolLayer(ProtocolLayer&&) = delete;
  ProtocolLayer& operator=(const ProtocolLayer&) = delete;
  ProtocolLayer& operator=(ProtocolLayer&&) = delete;
  ~ProtocolLayer() override = default;

  void Process(std::string_view) override;
  void UpgradeToWebsocket() override;

private:
  ConcreteHttpParser httpParser;
  ConcreteHttpSender httpSender;
  ConcreteWebsocketParser websocketParser;
  ConcreteWebsocketSender websocketSender;
  std::unique_ptr<Router> router;
  std::unique_ptr<ProtocolProcessor> processor;
  std::string buffer;
};

class ProtocolLayerFactory : public TcpProcessorFactory {
public:
  explicit ProtocolLayerFactory(RouterFactory&);
  ProtocolLayerFactory(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory(ProtocolLayerFactory&&) = delete;
  ProtocolLayerFactory& operator=(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory& operator=(ProtocolLayerFactory&&) = delete;
  ~ProtocolLayerFactory() override = default;

  std::unique_ptr<TcpProcessor> Create(TcpSender&) const override;

private:
  RouterFactory& routerFactory;
};

}  // namespace network
