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
  virtual void MarkSenderPending(int) = 0;
  virtual void UnmarkSenderPending(int) = 0;
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

struct HttpOptions {
  size_t maxPayloadSize;
};

using HttpQuery = std::unordered_map<std::string, std::string>;

struct HttpHeader {
  std::string field;
  std::string value;
};

using HttpHeaders = std::unordered_map<std::string, std::string>;

struct HttpRequest {
  std::string method;
  std::string uri;
  std::string version;
  HttpHeaders headers;
  HttpQuery query;
  std::string body;
};

enum class HttpStatus { OK, BadRequest, NotFound };

struct PreparedHttpResponse {
  HttpStatus status;
  HttpHeaders headers;
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

class HttpSender {
public:
  virtual ~HttpSender() = default;
  virtual void Send(PreparedHttpResponse&&) = 0;
  virtual void Send(FileHttpResponse&&) = 0;
  virtual void Send(MixedReplaceHeaderHttpResponse&&) = 0;
  virtual void Send(MixedReplaceDataHttpResponse&&) = 0;
  virtual void Send(ChunkedHeaderHttpResponse&&) = 0;
  virtual void Send(ChunkedDataHttpResponse&&) = 0;
  virtual void Close() = 0;
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

}  // namespace network
