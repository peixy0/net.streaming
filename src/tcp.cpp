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
#include <chrono>

namespace network {

TcpSender::TcpSender(int s) : peerDescriptor{s} {
  int flag = 0;
  int r = setsockopt(peerDescriptor, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
  if (r < 0) {
    spdlog::error("tcp setsockopt(): {}", strerror(errno));
  }
}

TcpSender::~TcpSender() {
  Close();
}

void TcpSender::Send(std::string_view buf) {
  size_t sent = 0;
  size_t size = buf.length();
  const std::string& s{buf.cbegin(), buf.cend()};
  while (sent < size) {
    int n = send(peerDescriptor, s.c_str() + sent, size - sent, 0);
    if (n == -1) {
      if (errno == EAGAIN or errno == EWOULDBLOCK) {
        continue;
      }
      spdlog::error("tcp send(): {}", strerror(errno));
      return;
    }
    sent += n;
  }
}

void TcpSender::SendFile(int fd, size_t size) {
  size_t sent = 0;
  while (sent < size) {
    int n = sendfile(peerDescriptor, fd, nullptr, size);
    if (n < 0) {
      if (errno == EAGAIN or errno == EWOULDBLOCK) {
        continue;
      }
      spdlog::error("tcp sendfile(): {}", strerror(errno));
      return;
    }
    sent += n;
  }
}

void TcpSender::Close() {
  if (peerDescriptor > 0) {
    shutdown(peerDescriptor, SHUT_RDWR);
    peerDescriptor = -1;
  }
}

TcpConnectionContext::TcpConnectionContext(int fd, std::unique_ptr<NetworkLayer> upperlayer,
                                           std::unique_ptr<TcpSender> sender)
    : fd{fd}, upperlayer{std::move(upperlayer)}, sender{std::move(sender)} {
  spdlog::debug("tcp connection established: {}", fd);
  UpdateTimeout();
}

TcpConnectionContext::~TcpConnectionContext() {
  spdlog::debug("tcp connection closed: {}", fd);
}

NetworkLayer& TcpConnectionContext::GetUpperlayer() {
  return *upperlayer;
}

TcpSender& TcpConnectionContext::GetTcpSender() {
  return *sender;
}

int TcpConnectionContext::GetTimeout() const {
  auto d = expire - std::chrono::system_clock::now();
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  auto t = s.count();
  return t > 0 ? t : 0;
}

void TcpConnectionContext::UpdateTimeout() {
  using namespace std::chrono_literals;
  expire = std::chrono::system_clock::now() + 20s;
}

TcpLayer::TcpLayer(NetworkLayerFactory& networkLayerFactory) : networkLayerFactory{networkLayerFactory} {
}

TcpLayer::~TcpLayer() {
  if (localDescriptor > 0) {
    close(localDescriptor);
    localDescriptor = -1;
  }
  if (epollDescriptor > 0) {
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

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = localDescriptor;
  int r = epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, localDescriptor, &event);
  if (r < 0) {
    spdlog::error("tcp epoll_ctl(): {}", strerror(errno));
    return;
  }

  constexpr int maxEvents = 32;
  epoll_event events[maxEvents];
  while (true) {
    int t = FindMostRecentTimeout();
    int n = epoll_wait(epollDescriptor, events, maxEvents, t);
    for (int i = 0; i < n; i++) {
      if (events[i].data.fd == localDescriptor) {
        SetupPeer();
        continue;
      }
      if (events[i].events & EPOLLIN) {
        ReadFromPeer(events[i].data.fd);
        continue;
      }
    }
    PurgeExpiredConnections();
  }
}

void TcpLayer::PurgeExpiredConnections() {
  for (auto& [_, context] : connections) {
    if (context->GetTimeout() <= 0) {
      context->GetTcpSender().Close();
    }
  }
}

int TcpLayer::FindMostRecentTimeout() {
  if (connections.empty()) {
    return -1;
  }
  auto it = connections.begin();
  int r = std::get<std::unique_ptr<TcpConnectionContext>>(*it)->GetTimeout();
  while (++it != connections.end()) {
    int t = std::get<std::unique_ptr<TcpConnectionContext>>(*it)->GetTimeout();
    if (r > t) {
      r = t;
    }
  }
  return r;
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

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = s;
  epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, s, &event);
  auto sender = std::make_unique<TcpSender>(s);
  auto upperlayer = networkLayerFactory.Create(*sender);
  auto context = std::make_unique<TcpConnectionContext>(s, std::move(upperlayer), std::move(sender));
  connections.emplace(s, std::move(context));
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
    spdlog::error("tcp recv(): {}", strerror(errno));
    ClosePeer(peerDescriptor);
    return;
  }
  if (r == 0) {
    ClosePeer(peerDescriptor);
    return;
  }
  auto& context = std::get<std::unique_ptr<TcpConnectionContext>>(*it);
  auto& upperlayer = context->GetUpperlayer();
  upperlayer.Receive({buf, buf + r});
}

Tcp4Layer::Tcp4Layer(std::string_view host, std::uint16_t port, NetworkLayerFactory& networkLayerFactory)
    : TcpLayer{networkLayerFactory}, host{host}, port{port} {
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
  if (s >= 0) {
    close(s);
  }
  return -1;
}

}  // namespace network
