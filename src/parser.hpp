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

}  // namespace network
