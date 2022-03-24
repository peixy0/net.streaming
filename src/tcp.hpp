#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include "network.hpp"

namespace network {

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

class TcpConnectionContext {
public:
  TcpConnectionContext(int, std::unique_ptr<NetworkLayer>, std::unique_ptr<TcpSender>);
  ~TcpConnectionContext();
  TcpConnectionContext(const TcpConnectionContext&) = delete;
  TcpConnectionContext(TcpConnectionContext&&) = delete;
  TcpConnectionContext& operator=(const TcpConnectionContext&) = delete;
  TcpConnectionContext& operator=(TcpConnectionContext&&) = delete;

  NetworkLayer& GetUpperlayer();

  TcpSender& GetTcpSender();
  int GetTimeout() const;
  void UpdateTimeout();

private:
  int fd;
  std::unique_ptr<NetworkLayer> upperlayer;
  std::unique_ptr<TcpSender> sender;
  std::chrono::system_clock::time_point expire;
};

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
  void PurgeExpiredConnections();
  int FindMostRecentTimeout();

  int localDescriptor{-1};
  int epollDescriptor{-1};
  std::unordered_map<int, std::unique_ptr<TcpConnectionContext>> connections;
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

}  // namespace network
