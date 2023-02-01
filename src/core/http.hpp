#pragma once
#include <optional>
#include "network.hpp"

namespace network {

class ConcreteHttpParser final : public HttpParser {
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

class ConcreteHttpSender final : public HttpSender {
public:
  explicit ConcreteHttpSender(TcpSender&);
  void Send(HttpResponse&&) const override;
  void Send(FileHttpResponse&&) const override;
  void Send(MixedReplaceHeaderHttpResponse&&) const override;
  void Send(MixedReplaceDataHttpResponse&&) const override;
  void Send(ChunkedHeaderHttpResponse&&) const override;
  void Send(ChunkedDataHttpResponse&&) const override;
  void Close() const override;

private:
  TcpSender& sender;
};

class HttpLayer final : public ProtocolProcessor {
public:
  HttpLayer(HttpParser&, HttpSender&, HttpProcessor&);
  HttpLayer(const HttpLayer&) = delete;
  HttpLayer(HttpLayer&&) = delete;
  HttpLayer& operator=(const HttpLayer&) = delete;
  HttpLayer& operator=(HttpLayer&&) = delete;
  ~HttpLayer() override = default;

  bool TryProcess(std::string&) const override;

private:
  HttpParser& parser;
  HttpSender& sender;
  HttpProcessor& processor;
};

}  // namespace network
