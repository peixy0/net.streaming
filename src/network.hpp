#pragma once
#include <memory>
#include <string>
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

class TcpLayer {
public:
  TcpLayer(std::string_view host, std::uint16_t port, NetworkLayerFactory&);
  TcpLayer(const TcpLayer&) = delete;
  TcpLayer(TcpLayer&&) = delete;
  TcpLayer& operator=(const TcpLayer&) = delete;
  TcpLayer& operator=(TcpLayer&&) = delete;
  ~TcpLayer();

  void Start();

private:
  void SetupSocket();
  void ReadFromPeer();

  int localDescriptor{-1};
  std::string host;
  std::uint16_t port;
  NetworkLayerFactory& networkLayerFactory;
};

class TcpSender : public NetworkSender {
public:
  explicit TcpSender(int socket);
  TcpSender(const TcpSender&) = delete;
  TcpSender(TcpSender&&) = delete;
  TcpSender& operator=(const TcpSender&) = delete;
  TcpSender& operator=(TcpSender&&) = delete;
  ~TcpSender();

  void Send(std::string_view) override;
  void Close() override;

private:
  int peerDescriptor;
};

}  // namespace network
