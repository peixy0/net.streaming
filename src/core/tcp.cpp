#include "tcp.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

TcpSendBuffer::TcpSendBuffer(int peer, std::string_view buffer) : peer{peer}, buffer{buffer}, size{buffer.size()} {
}

void TcpSendBuffer::Send() {
  while (size > 0) {
    int n = send(peer, buffer.c_str(), size, 0);
    if (n == -1) {
      if (errno == EAGAIN or errno == EWOULDBLOCK) {
        return;
      }
      spdlog::error("tcp send(): {}", strerror(errno));
      return;
    }
    if (n == 0) {
      return;
    }
    size -= n;
    buffer.erase(0, n);
  }
}

bool TcpSendBuffer::Done() const {
  return size == 0;
}

TcpSendFile::TcpSendFile(int peer, os::File file_) : peer{peer}, file{std::move(file_)} {
  if (not file.Ok()) {
    size = 0;
    return;
  }
  size = file.Size();
}

void TcpSendFile::Send() {
  while (size > 0) {
    int n = sendfile(peer, file.Fd(), nullptr, size);
    if (n < 0) {
      if (errno == EAGAIN or errno == EWOULDBLOCK) {
        return;
      }
      spdlog::error("tcp sendfile(): {}", strerror(errno));
      return;
    }
    if (n == 0) {
      return;
    }
    size -= n;
  }
}

bool TcpSendFile::Done() const {
  return size == 0;
}

TcpSendStream::TcpSendStream(int peer, TcpSender& sender, std::unique_ptr<RawStream> stream)
    : peer{peer}, sender{sender}, stream{std::move(stream)} {
}

void TcpSendStream::Send() {
  if (size == 0) {
    FillBuffer();
  }
  while (size > 0) {
    int n = send(peer, buffer.c_str(), size, 0);
    if (n == -1) {
      if (errno == EAGAIN or errno == EWOULDBLOCK) {
        return;
      }
      spdlog::error("tcp send(): {}", strerror(errno));
      return;
    }
    if (n == 0) {
      return;
    }
    size -= n;
    buffer.erase(0, n);
  }
}

bool TcpSendStream::Done() const {
  return done;
}

void TcpSendStream::FillBuffer() {
  auto buf = stream->GetBuffered();
  if (not buf) {
    done = true;
    return;
  }
  buffer = std::move(*buf);
  size = buffer.size();
  if (size == 0) {
    sender.UnmarkPending();
  }
}

ConcreteTcpSender::ConcreteTcpSender(int s, TcpSenderSupervisor& supervisor) : peer{s}, supervisor{supervisor} {
  int flag = 0;
  int r = setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
  if (r < 0) {
    spdlog::error("tcp setsockopt(): {}", strerror(errno));
  }
}

ConcreteTcpSender::~ConcreteTcpSender() {
  Close();
}

void ConcreteTcpSender::SendBuffered() {
  while (not buffered.empty()) {
    auto& op = buffered.front();
    op->Send();
    if (not op->Done()) {
      return;
    }
    buffered.pop_front();
  }
  UnmarkPending();
}

void ConcreteTcpSender::Send(std::string_view buf) {
  buffered.emplace_back(std::make_unique<TcpSendBuffer>(peer, buf));
  MarkPending();
  SendBuffered();
}

void ConcreteTcpSender::Send(os::File file) {
  buffered.emplace_back(std::make_unique<TcpSendFile>(peer, std::move(file)));
  MarkPending();
  SendBuffered();
}

void ConcreteTcpSender::Send(std::unique_ptr<RawStream> stream) {
  buffered.emplace_back(std::make_unique<TcpSendStream>(peer, *this, std::move(stream)));
  MarkPending();
  SendBuffered();
}

void ConcreteTcpSender::Close() {
  if (peer != -1) {
    shutdown(peer, SHUT_RDWR);
    peer = -1;
  }
}

void ConcreteTcpSender::MarkPending() {
  if (pending) {
    return;
  }
  pending = true;
  supervisor.MarkSenderPending(peer);
}

void ConcreteTcpSender::UnmarkPending() {
  if (not pending) {
    return;
  }
  pending = false;
  supervisor.UnmarkSenderPending(peer);
}

TcpConnectionContext::TcpConnectionContext(
    int fd, std::unique_ptr<TcpReceiver> receiver, std::unique_ptr<TcpSender> sender)
    : fd{fd}, receiver{std::move(receiver)}, sender{std::move(sender)} {
  spdlog::info("tcp connection established: {}", fd);
}

TcpConnectionContext::~TcpConnectionContext() {
  spdlog::info("tcp connection closed: {}", fd);
}

TcpReceiver& TcpConnectionContext::GetReceiver() {
  return *receiver;
}

TcpSender& TcpConnectionContext::GetSender() {
  return *sender;
}

TcpLayer::TcpLayer(TcpReceiverFactory& receiverFactory) : receiverFactory{receiverFactory} {
}

TcpLayer::~TcpLayer() {
  if (localDescriptor != -1) {
    close(localDescriptor);
    localDescriptor = -1;
  }
  if (epollDescriptor != -1) {
    close(epollDescriptor);
    epollDescriptor = -1;
  }
}

void TcpLayer::Start() {
  localDescriptor = CreateSocket();
  if (localDescriptor < 0) {
    return;
  }
  StartLoop();
}

void TcpLayer::MarkReceiverPending(int peer) {
  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = peer;
  int r = epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, peer, &event);
  if (r < 0) {
    spdlog::error("tcp epoll_ctl(): {}", strerror(errno));
    return;
  }
}

void TcpLayer::MarkSenderPending(int peer) {
  spdlog::debug("tcp mark sender pending: {}", peer);
  epoll_event event;
  event.events = EPOLLIN | EPOLLOUT;
  event.data.fd = peer;
  int r = epoll_ctl(epollDescriptor, EPOLL_CTL_MOD, peer, &event);
  if (r < 0) {
    spdlog::error("tcp epoll_ctl(): {}", strerror(errno));
    return;
  }
}

void TcpLayer::UnmarkSenderPending(int peer) {
  spdlog::debug("tcp unmark sender pending: {}", peer);
  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = peer;
  int r = epoll_ctl(epollDescriptor, EPOLL_CTL_MOD, peer, &event);
  if (r < 0) {
    spdlog::error("tcp epoll_ctl(): {}", strerror(errno));
    return;
  }
}

void TcpLayer::SetNonBlocking(int s) {
  int flags = fcntl(s, F_GETFL);
  if (flags < 0) {
    spdlog::error("tcp fcntl(): {}", strerror(errno));
    return;
  }
  flags = fcntl(s, F_SETFL, flags | O_NONBLOCK);
  if (flags < 0) {
    spdlog::error("tcp fcntl(): {}", strerror(errno));
    return;
  }
}

void TcpLayer::StartLoop() {
  epollDescriptor = epoll_create1(0);
  if (epollDescriptor < 0) {
    spdlog::error("tcp epoll_create1(): {}", strerror(errno));
    return;
  }
  MarkReceiverPending(localDescriptor);
  constexpr int maxEvents = 32;
  epoll_event events[maxEvents];
  while (true) {
    int n = epoll_wait(epollDescriptor, events, maxEvents, -1);
    for (int i = 0; i < n; i++) {
      if (events[i].data.fd == localDescriptor) {
        SetupPeer();
        continue;
      }
      if (events[i].events & EPOLLIN) {
        ReadFromPeer(events[i].data.fd);
        continue;
      }
      if (events[i].events & EPOLLOUT) {
        SendToPeer(events[i].data.fd);
        continue;
      }
    }
  }
}

void TcpLayer::SetupPeer() {
  sockaddr_in peerAddr;
  socklen_t n = sizeof peerAddr;
  bzero(&peerAddr, n);
  int s = accept(localDescriptor, reinterpret_cast<sockaddr*>(&peerAddr), &n);
  if (s < 0) {
    spdlog::error("tcp accept(): {}", strerror(errno));
    return;
  }
  SetNonBlocking(s);
  MarkReceiverPending(s);

  auto sender = std::make_unique<ConcreteTcpSender>(s, *this);
  auto receiver = receiverFactory.Create(*sender);
  connections.try_emplace(s, s, std::move(receiver), std::move(sender));
}

void TcpLayer::ClosePeer(int peerDescriptor) {
  epoll_ctl(epollDescriptor, EPOLL_CTL_DEL, peerDescriptor, nullptr);
  close(peerDescriptor);
  connections.erase(peerDescriptor);
}

void TcpLayer::ReadFromPeer(int peerDescriptor) {
  auto it = connections.find(peerDescriptor);
  if (it == connections.end()) {
    spdlog::error("tcp read from unexpected peer: {}", peerDescriptor);
    return;
  }
  char buf[512];
  size_t size = sizeof buf;
  int r = recv(peerDescriptor, buf, size, 0);
  if (r < 0) {
    if (errno == EAGAIN or errno == EWOULDBLOCK) {
      return;
    }
    ClosePeer(peerDescriptor);
    return;
  }
  if (r == 0) {
    ClosePeer(peerDescriptor);
    return;
  }
  auto& context = std::get<TcpConnectionContext>(*it);
  auto& receiver = context.GetReceiver();
  receiver.Receive({buf, buf + r});
}

void TcpLayer::SendToPeer(int peerDescriptor) {
  auto it = connections.find(peerDescriptor);
  if (it == connections.end()) {
    spdlog::error("tcp send to unexpected peer: {}", peerDescriptor);
    return;
  }

  auto& context = std::get<TcpConnectionContext>(*it);
  context.GetSender().SendBuffered();
}

Tcp4Layer::Tcp4Layer(std::string_view host, std::uint16_t port, TcpReceiverFactory& receiverFactory)
    : TcpLayer{receiverFactory}, host{host}, port{port} {
}

int Tcp4Layer::CreateSocket() {
  int flag = 1;
  int r = -1;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    spdlog::error("tcp socket(): {}", strerror(errno));
    goto out;
  }
  r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag);
  if (r < 0) {
    spdlog::error("tcp setsockopt(): {}", strerror(errno));
    goto out;
  }
  SetNonBlocking(s);

  sockaddr_in localAddr;
  bzero(&localAddr, sizeof localAddr);
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = inet_addr(host.c_str());
  localAddr.sin_port = htons(port);
  r = bind(s, reinterpret_cast<sockaddr*>(&localAddr), sizeof localAddr);
  if (r < 0) {
    spdlog::error("tcp bind(): {}", strerror(errno));
    goto out;
  }

  r = listen(s, 0);
  if (r < 0) {
    spdlog::error("tcp listen(): {}", strerror(errno));
    goto out;
  }
  spdlog::info("tcp listening on {}:{}", host, port);
  return s;

out:
  if (s != -1) {
    close(s);
  }
  return -1;
}

}  // namespace network
