#pragma once
#include <optional>
#include "network.hpp"

namespace network {

class ConcreteWebsocketParser : public WebsocketParser {
public:
  ConcreteWebsocketParser() = default;
  ConcreteWebsocketParser(const ConcreteWebsocketParser&) = delete;
  ConcreteWebsocketParser(ConcreteWebsocketParser&&) = delete;
  ConcreteWebsocketParser& operator=(const ConcreteWebsocketParser&) = delete;
  ConcreteWebsocketParser& operator=(ConcreteWebsocketParser&&) = delete;
  ~ConcreteWebsocketParser() override = default;

  std::optional<WebsocketFrame> Parse(std::string&) const override;

private:
  static constexpr std::uint8_t headerLen = 2;
  static constexpr std::uint8_t maskLen = 4;
  static constexpr std::uint8_t ext1Len = 2;
  static constexpr std::uint8_t ext2Len = 8;
};

class ConcreteWebsocketSender : public WebsocketSender {
public:
  explicit ConcreteWebsocketSender(TcpSender&);
  ConcreteWebsocketSender(const ConcreteWebsocketSender&) = delete;
  ConcreteWebsocketSender(ConcreteWebsocketSender&&) = delete;
  ConcreteWebsocketSender& operator=(const ConcreteWebsocketSender&) = delete;
  ConcreteWebsocketSender& operator=(ConcreteWebsocketSender&&) = delete;
  ~ConcreteWebsocketSender() override = default;

  void Send(WebsocketFrame&&) const override;
  void Close() const override;

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
  WebsocketLayer(WebsocketParser&, WebsocketSender&, WebsocketProcessor&);
  WebsocketLayer(const WebsocketLayer&) = delete;
  WebsocketLayer(WebsocketLayer&&) = delete;
  WebsocketLayer& operator=(const WebsocketLayer&) = delete;
  WebsocketLayer& operator=(WebsocketLayer&&) = delete;
  ~WebsocketLayer() override;

  bool TryProcess(std::string&) const override;

private:
  WebsocketParser& parser;
  WebsocketSender& sender;
  WebsocketProcessor& processor;
};

}  // namespace network
