#pragma once
#include <map>
#include <optional>
#include "network.hpp"

namespace network {

class HttpParser {
public:
  virtual ~HttpParser() = default;
  virtual std::optional<HttpRequest> Parse() = 0;
  virtual void Append(std::string_view) = 0;
  virtual size_t GetLength() const = 0;
};

class ConcreteHttpParser : public HttpParser {
public:
  ConcreteHttpParser() = default;
  ConcreteHttpParser(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser(ConcreteHttpParser&&) = delete;
  ConcreteHttpParser& operator=(const ConcreteHttpParser&) = delete;
  ConcreteHttpParser& operator=(ConcreteHttpParser&&) = delete;
  ~ConcreteHttpParser() = default;

  std::optional<HttpRequest> Parse() override;
  void Append(std::string_view) override;
  size_t GetLength() const override;

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

  std::string payload;
  size_t receivedLength{0};
  std::optional<std::string> method;
  std::optional<std::string> uri;
  std::string uriBase;
  HttpQuery query;
  std::optional<std::string> version;
  bool requestLineEndingParsed{false};
  HttpHeaders headers;
  bool headersParsed{false};
  bool headersEndingParsed{false};
  size_t bodyRemaining{0};
};

}  // namespace network
