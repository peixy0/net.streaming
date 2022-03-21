#pragma once
#include <string>
#include "network.hpp"

namespace network {

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
