#pragma once
#include <optional>
#include "network.hpp"
#include "parser.hpp"

namespace network {

class ConcreteWebsocketSender : public WebsocketFrameSender {
public:
  explicit ConcreteWebsocketSender(TcpSender&);
  ConcreteWebsocketSender(const ConcreteWebsocketSender&) = delete;
  ConcreteWebsocketSender(ConcreteWebsocketSender&&) = delete;
  ConcreteWebsocketSender& operator=(const ConcreteWebsocketSender&) = delete;
  ConcreteWebsocketSender& operator=(ConcreteWebsocketSender&&) = delete;
  ~ConcreteWebsocketSender() override = default;

  void Send(WebsocketFrame&&) override;
  void Close() override;

private:
  TcpSender& sender;
};

class WebsocketHandshakeBuilder {
public:
  explicit WebsocketHandshakeBuilder(const HttpRequest&);
  std::optional<HttpResponse> Build() const;

private:
  const HttpRequest& request;
};

class WebsocketLayer : public ProtocolProcessor {
public:
  WebsocketLayer(std::unique_ptr<network::WebsocketFrameParser>, std::unique_ptr<network::WebsocketFrameSender>,
      WebsocketProcessorFactory&);
  WebsocketLayer(const WebsocketLayer&) = delete;
  WebsocketLayer(WebsocketLayer&&) = delete;
  WebsocketLayer& operator=(const WebsocketLayer&) = delete;
  WebsocketLayer& operator=(WebsocketLayer&&) = delete;
  ~WebsocketLayer() override;

  void Process(std::string&) override;

private:
  static constexpr std::uint8_t opClose = 8;

  std::unique_ptr<network::WebsocketFrameParser> parser;
  std::unique_ptr<network::WebsocketFrameSender> sender;
  std::unique_ptr<WebsocketProcessor> processor;
  std::string message;
};

class ConcreteWebsocketLayerFactory : public WebsocketLayerFactory {
public:
  explicit ConcreteWebsocketLayerFactory(std::unique_ptr<WebsocketProcessorFactory>);
  ConcreteWebsocketLayerFactory(const ConcreteWebsocketLayerFactory&) = delete;
  ConcreteWebsocketLayerFactory(ConcreteWebsocketLayerFactory&&) = delete;
  ConcreteWebsocketLayerFactory& operator=(const ConcreteWebsocketLayerFactory&) = delete;
  ConcreteWebsocketLayerFactory& operator=(ConcreteWebsocketLayerFactory&&) = delete;
  ~ConcreteWebsocketLayerFactory() override = default;
  std::unique_ptr<ProtocolProcessor> Create(TcpSender&) const override;

private:
  std::unique_ptr<WebsocketProcessorFactory> processorFactory;
};

}  // namespace network
