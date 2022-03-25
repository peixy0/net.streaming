#pragma once
#include <optional>
#include "network.hpp"
#include "parser.hpp"

namespace network {

class HttpResponseVisitor {
public:
  HttpResponseVisitor(NetworkSender&);
  void operator()(const PlainHttpResponse&) const;
  void operator()(const FileHttpResponse&) const;

private:
  NetworkSender& sender;
};

class HttpLayer : public network::NetworkLayer {
public:
  HttpLayer(std::unique_ptr<HttpParser> parser, HttpProcessor& processor, NetworkSender&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer() = default;
  void Receive(std::string_view) override;

private:
  std::unique_ptr<HttpParser> parser;
  HttpProcessor& processor;
  NetworkSender& sender;
  std::string receivedPayload;
  size_t payloadSize{0};
};

class HttpLayerFactory : public network::NetworkLayerFactory {
public:
  explicit HttpLayerFactory(HttpProcessor&);
  HttpLayerFactory(const HttpLayerFactory&) = delete;
  HttpLayerFactory(HttpLayerFactory&&) = delete;
  HttpLayerFactory& operator=(const HttpLayerFactory&) = delete;
  HttpLayerFactory& operator=(HttpLayerFactory&&) = delete;
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::NetworkLayer> Create(NetworkSender&) const override;

private:
  HttpProcessor& processor;
};

}  // namespace network
