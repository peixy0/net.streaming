#pragma once
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <variant>
#include "file.hpp"

namespace network {

class SenderNotifier {
public:
  virtual ~SenderNotifier() = default;
  virtual void MarkPending() = 0;
  virtual void UnmarkPending() = 0;
};

class RawStream {
public:
  virtual ~RawStream() = default;
  virtual std::optional<std::string> GetBuffered() = 0;
};

class RawStreamFactory {
public:
  virtual ~RawStreamFactory() = default;
  virtual std::unique_ptr<RawStream> GetStream(SenderNotifier&) = 0;
};

class TcpSenderSupervisor {
public:
  virtual ~TcpSenderSupervisor() = default;
  virtual void MarkSenderPending(int) = 0;
  virtual void UnmarkSenderPending(int) = 0;
};

class TcpSender : public SenderNotifier {
public:
  virtual ~TcpSender() = default;
  virtual void Send(std::string_view) = 0;
  virtual void Send(os::File) = 0;
  virtual void Send(std::unique_ptr<RawStream>) = 0;
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

using HttpQuery = std::unordered_map<std::string, std::string>;

enum class HttpStatus { OK, BadRequest, NotFound };

struct HttpRequest {
  std::string method;
  std::string uri;
  std::string version;
  HttpHeaders headers;
  HttpQuery query;
  std::string body;
};

struct PreparedHttpResponse {
  HttpStatus status;
  HttpHeaders headers;
  std::string body;
};

struct FileHttpResponse {
  HttpHeaders headers;
  std::string path;
};

struct RawStreamHttpResponse {
  std::unique_ptr<RawStreamFactory> streamFactory;
};

using HttpResponse = std::variant<PreparedHttpResponse, FileHttpResponse, RawStreamHttpResponse>;

class HttpProcessor {
public:
  virtual ~HttpProcessor() = default;
  virtual HttpResponse Process(const HttpRequest&) = 0;
};

}  // namespace network
