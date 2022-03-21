#pragma once
#include "network.hpp"

namespace network {

struct HttpOptions {
  std::uint32_t maxPayloadSize;
};

class HttpLayer : public network::NetworkLayer {
public:
  HttpLayer(const HttpOptions& options, network::NetworkSender&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
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
  explicit HttpLayerFactory(const HttpOptions&);
  HttpLayerFactory(const HttpLayerFactory&) = delete;
  HttpLayerFactory(HttpLayerFactory&&) = delete;
  HttpLayerFactory& operator=(const HttpLayerFactory&) = delete;
  HttpLayerFactory& operator=(HttpLayerFactory&&) = delete;
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::NetworkLayer> Create(network::NetworkSender&) const override;

private:
  HttpOptions options;
};

}  // namespace network
