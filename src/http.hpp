#pragma once
#include "network.hpp"

namespace protocol {

class HttpLayer : public network::NetworkLayer {
public:
  explicit HttpLayer(network::NetworkSender&);
  ~HttpLayer() = default;
  void Receive(std::string_view) override;

private:
  network::NetworkSender& sender;
  std::string receivedPayload;
  std::uint32_t payloadSize{0};
};

class HttpLayerFactory : public network::NetworkLayerFactory {
public:
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::NetworkLayer> Create(network::NetworkSender&) const override;
};

}  // namespace protocol
