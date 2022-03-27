#pragma once
#include <optional>
#include "network.hpp"
#include "parser.hpp"

namespace network {

class HttpResponseVisitor {
public:
  explicit HttpResponseVisitor(TcpSender&);
  void operator()(const PlainTextHttpResponse&) const;
  void operator()(const FileHttpResponse&) const;

private:
  TcpSender& sender;
};

class HttpLayer : public network::TcpReceiver {
public:
  HttpLayer(std::unique_ptr<HttpParser> parser, HttpProcessor& processor, TcpSender&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer() = default;
  void Receive(std::string_view) override;

private:
  std::unique_ptr<HttpParser> parser;
  HttpProcessor& processor;
  TcpSender& sender;
  std::string receivedPayload;
  size_t payloadSize{0};
};

class HttpLayerFactory : public network::TcpReceiverFactory {
public:
  explicit HttpLayerFactory(HttpProcessor&);
  HttpLayerFactory(const HttpLayerFactory&) = delete;
  HttpLayerFactory(HttpLayerFactory&&) = delete;
  HttpLayerFactory& operator=(const HttpLayerFactory&) = delete;
  HttpLayerFactory& operator=(HttpLayerFactory&&) = delete;
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::TcpReceiver> Create(TcpSender&) const override;

private:
  HttpProcessor& processor;
};

}  // namespace network
