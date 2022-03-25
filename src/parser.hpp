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
  ConcreteHttpParser() = default;
  ConcreteHttpParser(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser(ConcreteHttpParser&&) = delete;
  ConcreteHttpParser& operator=(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser& operator=(ConcreteHttpParser&&) = delete;
  ~ConcreteHttpParser() = default;

  std::optional<HttpRequest> Parse(std::string&);

private:
  void Reset();
  bool IsControl(char c) const;
  bool IsSeparator(char c) const;
  void SkipWhiteSpaces(std::string&) const;
  bool Consume(std::string&, std::string_view) const;
  std::optional<std::string> ParseToken(std::string&) const;
  bool ParseHeaders(std::string&, HttpHeaders&) const;
  std::optional<HttpHeader> ParseHeader(std::string&) const;
  std::optional<std::string> ParseHeaderField(std::string&) const;
  std::optional<std::string> ParseHeaderLine(std::string&) const;
  size_t ParseContentLength(const HttpHeaders&) const;

  std::optional<std::string> method;
  std::optional<std::string> uri;
  std::optional<std::string> version;
  bool requestLineEndingParsed{false};
  HttpHeaders headers;
  bool headersParsed{false};
  bool headersEndingParsed{false};
  size_t bodyRemaining{0};
};

}  // namespace network
