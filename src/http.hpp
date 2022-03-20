#pragma once
#include "network.hpp"

namespace protocol {

struct HttpOptions {
  std::uint32_t maxPayloadSize;
};

class HttpLayer : public network::NetworkLayer {
public:
  explicit HttpLayer(const HttpOptions& options, network::NetworkSender&);
  ~HttpLayer() = default;
  void Receive(std::string_view) override;

private:
  HttpOptions options;
  network::NetworkSender& sender;
  std::string receivedPayload;
  std::uint32_t payloadSize{0};
};

class HttpLayerFactory : public network::NetworkLayerFactory {
public:
  HttpLayerFactory(const HttpOptions&);
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::NetworkLayer> Create(network::NetworkSender&) const override;

private:
  HttpOptions options;
};

}  // namespace protocol
