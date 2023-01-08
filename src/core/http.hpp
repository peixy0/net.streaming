#pragma once
#include <optional>
#include "network.hpp"
#include "parser.hpp"

namespace network {

class HttpResponseVisitor {
public:
  explicit HttpResponseVisitor(TcpSender&);
  void operator()(PreparedHttpResponse&&) const;
  void operator()(FileHttpResponse&&) const;
  void operator()(RawStreamHttpResponse&&) const;

private:
  TcpSender& sender;
};

class HttpLayer : public network::TcpReceiver {
public:
  HttpLayer(const HttpOptions&, std::unique_ptr<HttpParser>, HttpProcessor&, TcpSender&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer() = default;
  void Receive(std::string_view) override;

private:
  HttpOptions options;
  std::unique_ptr<HttpParser> parser;
  HttpProcessor& processor;
  TcpSender& sender;
};

class HttpLayerFactory : public network::TcpReceiverFactory {
public:
  HttpLayerFactory(const HttpOptions&, HttpProcessor&);
  HttpLayerFactory(const HttpLayerFactory&) = delete;
  HttpLayerFactory(HttpLayerFactory&&) = delete;
  HttpLayerFactory& operator=(const HttpLayerFactory&) = delete;
  HttpLayerFactory& operator=(HttpLayerFactory&&) = delete;
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::TcpReceiver> Create(TcpSender&) const override;

private:
  HttpOptions options;
  HttpProcessor& processor;
};

}  // namespace network
