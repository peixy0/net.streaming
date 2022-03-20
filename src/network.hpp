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

class NetworkSupervisor {
public:
  virtual ~NetworkSupervisor() = default;
  virtual void OnPeerClose(int) = 0;
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

class Tcp4Layer : public NetworkSupervisor {
public:
  Tcp4Layer(std::string_view host, std::uint16_t port, NetworkLayerFactory&);
  Tcp4Layer(const Tcp4Layer&) = delete;
  Tcp4Layer(Tcp4Layer&&) = delete;
  Tcp4Layer& operator=(const Tcp4Layer&) = delete;
  Tcp4Layer& operator=(Tcp4Layer&&) = delete;
  ~Tcp4Layer();

  void Start();
  void OnPeerClose(int) override;

private:
  void SetupSocket();
  void SetupNonBlocking(int);
  void StartEpoll();
  void SetupPeer();
  void ReadFromPeer(int);

  int localDescriptor{-1};
  int epollDescriptor{-1};
  std::string host;
  std::uint16_t port;
  NetworkLayerFactory& networkLayerFactory;
};

class TcpSender : public NetworkSender {
public:
  TcpSender(int socket, NetworkSupervisor&);
  TcpSender(const TcpSender&) = delete;
  TcpSender(TcpSender&&) = delete;
  TcpSender& operator=(const TcpSender&) = delete;
  TcpSender& operator=(TcpSender&&) = delete;
  ~TcpSender();

  void Send(std::string_view) override;
  void Close() override;

private:
  int peerDescriptor;
  NetworkSupervisor& supervisor;
};

}  // namespace network
