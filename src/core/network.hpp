#pragma once
#include <memory>
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

class TcpReceiver {
public:
  virtual ~TcpReceiver() = default;
  virtual void Receive(std::string_view) = 0;
};

class TcpReceiverFactory {
public:
  virtual std::unique_ptr<TcpReceiver> Create(TcpSender&) const = 0;
};

struct HttpHeader {
  std::string field;
  std::string value;
};

struct HttpOptions {
  size_t maxPayloadSize;
};

using HttpHeaders = std::unordered_map<std::string, std::string>;

enum class HttpStatus { OK, NotFound };

struct HttpRequest {
  std::string method;
  std::string uri;
  std::string version;
  HttpHeaders headers;
  std::string body;
};

struct PlainTextHttpResponse {
  HttpStatus status;
  std::string body;
};

struct FileHttpResponse {
  std::string path;
  std::string contentType;
};

using HttpResponse = std::variant<PlainTextHttpResponse, FileHttpResponse>;

class HttpProcessor {
public:
  virtual ~HttpProcessor() = default;
  virtual HttpResponse Process(const HttpRequest&) = 0;
};

}  // namespace network
