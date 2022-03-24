#pragma once
#include <memory>
#include <string_view>

namespace network {

struct HttpRequest {
  std::string path;
};

struct HttpResponse {
  std::string body;
};

class HttpProcessor {
public:
  virtual ~HttpProcessor() = default;
  virtual HttpResponse Process(const HttpRequest&) = 0;
};

class NetworkSender {
public:
  virtual ~NetworkSender() = default;
  virtual void Send(std::string_view) = 0;
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
