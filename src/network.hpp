#pragma once
#include <memory>
#include <string_view>

namespace network {

class NetworkSender {
public:
  virtual ~NetworkSender() = default;
  virtual void Send(std::string_view buf) = 0;
  virtual void Close() = 0;
};

class NetworkLayer {
public:
  virtual ~NetworkLayer() = default;
  virtual void Receive(std::string_view buf) = 0;
};

class NetworkLayerFactory {
public:
  virtual std::unique_ptr<NetworkLayer> Create(NetworkSender&) const = 0;
};

}  // namespace network
