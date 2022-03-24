#pragma once
#include <optional>
#include "network.hpp"

namespace network {

class HttpParser {
public:
  virtual ~HttpParser() = default;
  virtual std::optional<HttpRequest> Parse(std::string&) = 0;
};

class ConcreteHttpParser : public HttpParser {
public:
  std::optional<HttpRequest> Parse(std::string&);

private:
  void Reset();
  void ToLower(std::string&) const;
  void SkipWhiteSpaces(std::string&) const;
  bool Consume(std::string&, std::string_view) const;
  std::optional<std::string> ParseToken(std::string&) const;
  bool ParseHeaders(std::string&, HttpHeaders&) const;
  std::optional<HttpHeader> ParseHeader(std::string&) const;
  std::optional<std::string> ParseLine(std::string&) const;
  std::uint32_t ParseContentLength(const HttpHeaders&) const;

  std::optional<std::string> method;
  std::optional<std::string> uri;
  std::optional<std::string> version;
  bool headerParsed{false};
  HttpHeaders headers;
  std::uint32_t bodyRemaining{0};
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
  std::optional<HttpRequest> TryParsePayload();

  std::unique_ptr<HttpParser> parser;
  HttpProcessor& processor;
  NetworkSender& sender;
  std::string receivedPayload;
  std::uint32_t payloadSize{0};
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
