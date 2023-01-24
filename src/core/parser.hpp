#pragma once
#include <map>
#include <optional>
#include "network.hpp"

namespace network {

class ConcreteHttpParser : public HttpParser {
public:
  ConcreteHttpParser() = default;
  ConcreteHttpParser(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser(ConcreteHttpParser&&) = delete;
  ConcreteHttpParser& operator=(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser& operator=(ConcreteHttpParser&&) = delete;
  ~ConcreteHttpParser() override = default;

  std::optional<HttpRequest> Parse(std::string&) const override;

private:
  void Reset();
  void SkipWhiteSpaces(std::string&) const;
  bool Consume(std::string&, std::string_view) const;
  std::optional<std::string> ParseToken(std::string&) const;
  bool ParseHeaders(std::string&, HttpHeaders&) const;
  std::optional<HttpHeader> ParseHeader(std::string&) const;
  std::optional<std::string> ParseHeaderField(std::string&) const;
  std::optional<std::string> ParseLine(std::string&) const;
  size_t FindContentLength(const HttpHeaders&) const;
  std::string ParseUriBase(std::string&) const;
  std::string ParseQueryKey(std::string&) const;
  std::string ParseQueryValue(std::string&) const;
  HttpQuery ParseQueryString(std::string&) const;
};

class ConcreteWebsocketFrameParser : public WebsocketFrameParser {
public:
  ConcreteWebsocketFrameParser() = default;
  ConcreteWebsocketFrameParser(const ConcreteWebsocketFrameParser&) = delete;
  ConcreteWebsocketFrameParser(ConcreteWebsocketFrameParser&&) = delete;
  ConcreteWebsocketFrameParser& operator=(const ConcreteWebsocketFrameParser&) = delete;
  ConcreteWebsocketFrameParser& operator=(ConcreteWebsocketFrameParser&&) = delete;
  ~ConcreteWebsocketFrameParser() override = default;

  std::optional<WebsocketFrame> Parse(std::string&) const override;

private:
  static constexpr std::uint8_t headerLen = 2;
  static constexpr std::uint8_t maskLen = 4;
  static constexpr std::uint8_t ext1Len = 2;
  static constexpr std::uint8_t ext2Len = 8;
};

}  // namespace network
