#pragma once
#include "network.hpp"
#include "parser.hpp"

namespace network {

class ConcreteHttpSender : public HttpSender {
public:
  ConcreteHttpSender(TcpSender&);
  void Send(PreparedHttpResponse&&) override;
  void Send(FileHttpResponse&&) override;
  void Send(MixedReplaceHeaderHttpResponse&&) override;
  void Send(MixedReplaceDataHttpResponse&&) override;
  void Send(ChunkedHeaderHttpResponse&&) override;
  void Send(ChunkedDataHttpResponse&&) override;
  void Close() override;

private:
  TcpSender& sender;
};

class HttpLayer : public network::TcpProcessor {
public:
  HttpLayer(
      const HttpOptions&, std::unique_ptr<HttpParser>, std::unique_ptr<HttpSender>, std::unique_ptr<HttpProcessor>);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer();
  void Process(std::string_view) override;

private:
  HttpOptions options;
  std::unique_ptr<HttpParser> parser;
  std::unique_ptr<HttpSender> sender;
  std::unique_ptr<HttpProcessor> processor;
};

class HttpLayerFactory : public network::TcpProcessorFactory {
public:
  HttpLayerFactory(const HttpOptions&, HttpProcessorFactory&);
  HttpLayerFactory(const HttpLayerFactory&) = delete;
  HttpLayerFactory(HttpLayerFactory&&) = delete;
  HttpLayerFactory& operator=(const HttpLayerFactory&) = delete;
  HttpLayerFactory& operator=(HttpLayerFactory&&) = delete;
  ~HttpLayerFactory() = default;
  std::unique_ptr<network::TcpProcessor> Create(TcpSender&) const override;

private:
  HttpOptions options;
  HttpProcessorFactory& processorFactory;
};

}  // namespace network
