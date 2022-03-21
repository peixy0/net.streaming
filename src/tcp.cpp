#include "tcp.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

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
    }
  }
}

void TcpLayer::SetupPeer() {
  sockaddr_in peerAddr;
  unsigned int n = sizeof peerAddr;
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
}

void TcpLayer::ReadFromPeer(int peer) {
  auto sender = std::make_unique<TcpSender>(peer);
  auto upperLayer = networkLayerFactory.Create(*sender);
  char buf[512];
  int size = sizeof buf;
  int r = 0;
  do {
    r = recv(peer, buf, size, 0);
    if (r < 0) {
      return;
    }
    upperLayer->Receive({buf, buf + r});
  } while (r > 0);
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
  const std::string& s{buf.cbegin(), buf.cend()};
  send(peerDescriptor, s.c_str(), s.length(), 0);
}

void TcpSender::Close() {
  if (peerDescriptor > 0) {
    close(peerDescriptor);
    peerDescriptor = -1;
  }
}

}  // namespace network
