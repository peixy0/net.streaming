#pragma once
#include <string>
#include <unordered_map>
#include "network.hpp"

namespace network {

class TcpLayer {
public:
  explicit TcpLayer(NetworkLayerFactory&);
  TcpLayer(const TcpLayer&) = delete;
  TcpLayer(TcpLayer&&) = delete;
  TcpLayer& operator=(const TcpLayer&) = delete;
  TcpLayer& operator=(TcpLayer&&) = delete;
  virtual ~TcpLayer();

  void Start();

protected:
  virtual int CreateSocket() = 0;
  void SetNonBlocking(int);

private:
  void StartLoop();
  void SetupPeer();
  void ClosePeer(int);
  void ReadFromPeer(int);

  int localDescriptor{-1};
  int epollDescriptor{-1};
  std::unordered_map<int, std::unique_ptr<NetworkLayer>> connections;
  NetworkLayerFactory& networkLayerFactory;
};

class Tcp4Layer : public TcpLayer {
public:
  Tcp4Layer(std::string_view, std::uint16_t, NetworkLayerFactory&);

protected:
  int CreateSocket() override;

private:
  std::string host;
  std::uint16_t port;
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
