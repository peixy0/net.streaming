#pragma once
#include "network.hpp"
#include "parser.hpp"

namespace network {

class ConcreteHttpSender : public HttpSender {
public:
  ConcreteHttpSender(TcpSender&);
  void Send(HttpResponse&&) override;
  void Send(FileHttpResponse&&) override;
  void Send(MixedReplaceHeaderHttpResponse&&) override;
  void Send(MixedReplaceDataHttpResponse&&) override;
  void Send(ChunkedHeaderHttpResponse&&) override;
  void Send(ChunkedDataHttpResponse&&) override;
  void Close() override;

private:
  TcpSender& sender;
};

class HttpLayer : public network::ProtocolProcessor {
public:
  HttpLayer(std::unique_ptr<HttpParser>, std::unique_ptr<HttpSender>, ProtocolUpgrader&, HttpProcessorFactory&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer() override;
  bool TryProcess(std::string&) override;

private:
  std::unique_ptr<HttpParser> parser;
  std::unique_ptr<HttpSender> sender;
  ProtocolUpgrader& upgrader;
  HttpProcessorFactory& processorFactory;
  std::unique_ptr<HttpProcessor> processor;
};

class ConcreteHttpLayerFactory : public network::HttpLayerFactory {
public:
  explicit ConcreteHttpLayerFactory(std::unique_ptr<HttpProcessorFactory>);
  ConcreteHttpLayerFactory(const ConcreteHttpLayerFactory&) = delete;
  ConcreteHttpLayerFactory(ConcreteHttpLayerFactory&&) = delete;
  ConcreteHttpLayerFactory& operator=(const ConcreteHttpLayerFactory&) = delete;
  ConcreteHttpLayerFactory& operator=(ConcreteHttpLayerFactory&&) = delete;
  ~ConcreteHttpLayerFactory() override = default;
  std::unique_ptr<network::ProtocolProcessor> Create(TcpSender&, ProtocolUpgrader&) const override;

private:
  std::unique_ptr<HttpProcessorFactory> processorFactory;
};

}  // namespace network
