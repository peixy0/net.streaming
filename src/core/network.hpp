#pragma once
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <variant>
#include "file.hpp"

namespace network {

class TcpSenderSupervisor {
public:
  virtual ~TcpSenderSupervisor() = default;
  virtual void MarkSenderPending(int) const = 0;
  virtual void UnmarkSenderPending(int) const = 0;
};

class TcpSender {
public:
  virtual ~TcpSender() = default;
  virtual void Send(std::string_view) = 0;
  virtual void Send(os::File) = 0;
  virtual void SendBuffered() = 0;
  virtual void Close() = 0;
};

class TcpProcessor {
public:
  virtual ~TcpProcessor() = default;
  virtual void Process(std::string_view) = 0;
};

class TcpProcessorFactory {
public:
  virtual ~TcpProcessorFactory() = default;
  virtual std::unique_ptr<TcpProcessor> Create(TcpSender&) const = 0;
};

class ProtocolUpgrader {
public:
  virtual ~ProtocolUpgrader() = default;
  virtual void UpgradeToWebsocket() = 0;
};

class ProtocolProcessor {
public:
  virtual ~ProtocolProcessor() = default;
  virtual bool TryProcess(std::string&) const = 0;
};

enum class HttpMethod { PUT, GET, POST, DELETE };

using HttpQuery = std::unordered_map<std::string, std::string>;

struct HttpHeader {
  std::string field;
  std::string value;
};

using HttpHeaders = std::unordered_map<std::string, std::string>;

struct HttpRequest {
  HttpMethod method;
  std::string uri;
  std::string version;
  HttpHeaders headers;
  HttpQuery query;
  std::string body;
};

enum class HttpStatus { SwitchingProtocols, OK, BadRequest, NotFound };

struct HttpResponse {
  HttpStatus status;
  HttpHeaders headers;
  std::string body;
};

struct RawHttpResponse {
  std::string body;
};

struct FileHttpResponse {
  HttpHeaders headers;
  std::string path;
};

struct MixedReplaceHeaderHttpResponse {};

struct MixedReplaceDataHttpResponse {
  HttpHeaders headers;
  std::string body;
};

struct ChunkedHeaderHttpResponse {
  HttpHeaders headers;
};

struct ChunkedDataHttpResponse {
  std::string body;
};

class HttpParser {
public:
  virtual ~HttpParser() = default;
  virtual std::optional<HttpRequest> Parse(std::string&) const = 0;
};

class HttpSender {
public:
  virtual ~HttpSender() = default;
  virtual void Send(HttpResponse&&) const = 0;
  virtual void Send(FileHttpResponse&&) const = 0;
  virtual void Send(MixedReplaceHeaderHttpResponse&&) const = 0;
  virtual void Send(MixedReplaceDataHttpResponse&&) const = 0;
  virtual void Send(ChunkedHeaderHttpResponse&&) const = 0;
  virtual void Send(ChunkedDataHttpResponse&&) const = 0;
  virtual void Close() const = 0;
};

class HttpProcessor {
public:
  virtual ~HttpProcessor() = default;
  virtual void Process(HttpRequest&&) = 0;
};

class HttpProcessorFactory {
public:
  virtual ~HttpProcessorFactory() = default;
  virtual std::unique_ptr<HttpProcessor> Create(HttpSender&) const = 0;
};

struct WebsocketFrame {
  bool fin;
  std::uint8_t opcode;
  std::string payload;
};

class WebsocketParser {
public:
  virtual ~WebsocketParser() = default;
  virtual std::optional<WebsocketFrame> Parse(std::string&) const = 0;
};

class WebsocketSender {
public:
  virtual ~WebsocketSender() = default;
  virtual void Send(WebsocketFrame&&) const = 0;
  virtual void Close() const = 0;
};

class WebsocketProcessor {
public:
  virtual ~WebsocketProcessor() = default;
  virtual void Process(WebsocketFrame&&) = 0;
};

class WebsocketProcessorFactory {
public:
  virtual ~WebsocketProcessorFactory() = default;
  virtual std::unique_ptr<WebsocketProcessor> Create(WebsocketSender&) const = 0;
};

class Router : public HttpProcessor, public WebsocketProcessor {
public:
  ~Router() override = default;
};

class RouterFactory {
public:
  virtual ~RouterFactory() = default;
  virtual std::unique_ptr<Router> Create(HttpSender&, WebsocketSender&, ProtocolUpgrader&) const = 0;
};

}  // namespace network
