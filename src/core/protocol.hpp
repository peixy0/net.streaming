#pragma once
#include <memory>
#include "network.hpp"

namespace network {

class ProtocolLayer final : public TcpProcessor, public ProtocolDispatcher {
public:
  ProtocolLayer(TcpSender& sender, RouterFactory& routerFactory) : router{routerFactory.Create(sender, *this)} {
  }
  ProtocolLayer(const ProtocolLayer&) = delete;
  ProtocolLayer(ProtocolLayer&&) = delete;
  ProtocolLayer& operator=(const ProtocolLayer&) = delete;
  ProtocolLayer& operator=(ProtocolLayer&&) = delete;
  ~ProtocolLayer() override = default;

  void Process(std::string_view payload) override {
    buffer += payload;
    if (not processor) {
      return;
    }
    while (processor->TryProcess(buffer)) {
    }
  }

  void SetProcessor(ProtocolProcessor* processor_) override {
    processor = processor_;
  }

private:
  std::string buffer;
  ProtocolProcessor* processor{nullptr};
  std::unique_ptr<Router> router;
};

class ProtocolLayerFactory final : public TcpProcessorFactory {
public:
  explicit ProtocolLayerFactory(RouterFactory& routerFactory) : routerFactory{routerFactory} {
  }
  ProtocolLayerFactory(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory(ProtocolLayerFactory&&) = delete;
  ProtocolLayerFactory& operator=(const ProtocolLayerFactory&) = delete;
  ProtocolLayerFactory& operator=(ProtocolLayerFactory&&) = delete;
  ~ProtocolLayerFactory() override = default;

  std::unique_ptr<TcpProcessor> Create(TcpSender& sender) const override {
    return std::make_unique<ProtocolLayer>(sender, routerFactory);
  }

private:
  RouterFactory& routerFactory;
};

}  // namespace network
