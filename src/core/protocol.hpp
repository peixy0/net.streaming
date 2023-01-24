#pragma once
#include <memory>
#include "network.hpp"

namespace network {

class ProtocolLayer : public TcpProcessor, public ProtocolUpgrader {
public:
  explicit ProtocolLayer(TcpSender&, HttpLayerFactory&);
  ProtocolLayer(const ProtocolLayer&) = delete;
  ProtocolLayer(ProtocolLayer&&) = delete;
  ProtocolLayer& operator=(const ProtocolLayer&) = delete;
  ProtocolLayer& operator=(ProtocolLayer&&) = delete;
  ~ProtocolLayer() override = default;

  void Process(std::string_view) override;
  void Add(WebsocketLayerFactory*);
  void UpgradeToWebsocket() override;

private:
  TcpSender& sender;
  HttpLayerFactory& httpLayerFactory;
  std::unique_ptr<ProtocolProcessor> processor;
  WebsocketLayerFactory* websocketLayerFactory{nullptr};
  std::string buffer;
};

class ProtocolLayerFactory : public TcpProcessorFactory {
public:
  explicit ProtocolLayerFactory(std::unique_ptr<HttpLayerFactory>);
  ProtocolLayerFactory(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory(ProtocolLayerFactory&&) = delete;
  ProtocolLayerFactory& operator=(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory& operator=(ProtocolLayerFactory&&) = delete;
  ~ProtocolLayerFactory() override = default;

  std::unique_ptr<TcpProcessor> Create(TcpSender&) const override;
  void Add(std::unique_ptr<WebsocketLayerFactory>);

private:
  std::unique_ptr<HttpLayerFactory> httpLayerFactory{nullptr};
  std::unique_ptr<WebsocketLayerFactory> websocketLayerFactory{nullptr};
};

}  // namespace network
