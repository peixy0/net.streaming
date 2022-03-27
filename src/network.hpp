#pragma once
#include <memory>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace network {

struct HttpHeader {
  std::string field;
  std::string value;
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
  std::string contentType;
  std::string path;
};

using HttpResponse = std::variant<PlainTextHttpResponse, FileHttpResponse>;

class HttpProcessor {
public:
  virtual ~HttpProcessor() = default;
  virtual HttpResponse Process(const HttpRequest&) = 0;
};

class NetworkSender {
public:
  virtual ~NetworkSender() = default;
  virtual void Send(std::string_view) = 0;
  virtual void SendFile(int, size_t) = 0;
  virtual void Close() = 0;
};

class NetworkLayer {
public:
  virtual ~NetworkLayer() = default;
  virtual void Receive(std::string_view) = 0;
};

class NetworkLayerFactory {
public:
  virtual std::unique_ptr<NetworkLayer> Create(NetworkSender&) const = 0;
};

}  // namespace network
