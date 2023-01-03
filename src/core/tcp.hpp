#pragma once
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include "network.hpp"

namespace network {

class TcpSendOp {
public:
  virtual ~TcpSendOp() = default;
  virtual void Send() = 0;
  virtual bool Done() const = 0;
};

class TcpSendBuffer : public TcpSendOp {
public:
  TcpSendBuffer(int, std::string_view);
  void Send() override;
  bool Done() const override;

private:
  int peer;
  std::string buffer;
  size_t size;
};

class TcpSendFile : public TcpSendOp {
public:
  TcpSendFile(int, os::File file);
  void Send() override;
  bool Done() const override;

private:
  int peer;
  os::File file;
  size_t size;
};

class ConcreteTcpSender : public TcpSender {
public:
  ConcreteTcpSender(int, TcpSenderSupervisor&);
  ConcreteTcpSender(const ConcreteTcpSender&) = delete;
  ConcreteTcpSender(ConcreteTcpSender&&) = delete;
  ConcreteTcpSender& operator=(const ConcreteTcpSender&) = delete;
  ConcreteTcpSender& operator=(ConcreteTcpSender&&) = delete;
  ~ConcreteTcpSender();

  void Send(std::string_view) override;
  void Send(os::File) override;
  void SendBuffered() override;
  void Close() override;

private:
  int peer;
  TcpSenderSupervisor& supervisor;
  std::deque<std::unique_ptr<TcpSendOp>> buffered;
  bool pending{false};
};

class TcpConnectionContext {
public:
  TcpConnectionContext(int, std::unique_ptr<TcpReceiver>, std::unique_ptr<TcpSender>);
  ~TcpConnectionContext();
  TcpConnectionContext(const TcpConnectionContext&) = delete;
  TcpConnectionContext(TcpConnectionContext&&) = delete;
  TcpConnectionContext& operator=(const TcpConnectionContext&) = delete;
  TcpConnectionContext& operator=(TcpConnectionContext&&) = delete;

  TcpReceiver& GetReceiver();
  TcpSender& GetSender();

private:
  int fd;
  std::unique_ptr<TcpReceiver> receiver;
  std::unique_ptr<TcpSender> sender;
};

class TcpLayer : public TcpSenderSupervisor {
public:
  explicit TcpLayer(TcpReceiverFactory&);
  TcpLayer(const TcpLayer&) = delete;
  TcpLayer(TcpLayer&&) = delete;
  TcpLayer& operator=(const TcpLayer&) = delete;
  TcpLayer& operator=(TcpLayer&&) = delete;
  virtual ~TcpLayer();

  void Start();
  void MarkSenderPending(int) override;
  void UnmarkSenderPending(int) override;

protected:
  virtual int CreateSocket() = 0;
  void SetNonBlocking(int);

private:
  void StartLoop();
  void MarkReceiverPending(int);
  void SetupPeer();
  void ClosePeer(int);
  void ReadFromPeer(int);
  void SendToPeer(int);
  void PurgeExpiredConnections();
  int FindMostRecentTimeout() const;

  int localDescriptor{-1};
  int epollDescriptor{-1};
  std::unordered_map<int, TcpConnectionContext> connections;
  TcpReceiverFactory& receiverFactory;
};

class Tcp4Layer : public TcpLayer {
public:
  Tcp4Layer(std::string_view, std::uint16_t, TcpReceiverFactory&);

protected:
  int CreateSocket() override;

private:
  std::string host;
  std::uint16_t port;
};

}  // namespace network
