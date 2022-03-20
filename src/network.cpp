#include "network.hpp"
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

Tcp4Layer::Tcp4Layer(std::string_view host, std::uint16_t port, NetworkLayerFactory& networkLayerFactory)
    : host{host}, port{port}, networkLayerFactory{networkLayerFactory} {
}

Tcp4Layer::~Tcp4Layer() {
  if (localDescriptor > 0) {
    close(localDescriptor);
    localDescriptor = -1;
  }
  if (epollDescriptor > 0) {
    close(epollDescriptor);
    epollDescriptor = -1;
  }
}

void Tcp4Layer::Start() {
  SetupSocket();
  StartEpoll();
}

void Tcp4Layer::SetupSocket() {
  localDescriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (localDescriptor < 0) {
    spdlog::error("TCP4 socket(): {}", strerror(errno));
    return;
  }
  int flag = 1;
  int r = setsockopt(localDescriptor, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag);
  if (r < 0) {
    spdlog::error("TCP4 setsockopt(): {}", strerror(errno));
    return;
  }
  SetupNonBlocking(localDescriptor);

  sockaddr_in localAddr;
  bzero(&localAddr, sizeof localAddr);
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = inet_addr(host.c_str());
  localAddr.sin_port = htons(port);
  r = bind(localDescriptor, reinterpret_cast<sockaddr*>(&localAddr), sizeof localAddr);
  if (r < 0) {
    spdlog::error("TCP4 bind(): {}", strerror(errno));
    return;
  }

  r = listen(localDescriptor, 0);
  if (r < 0) {
    spdlog::error("TCP4 listen(): {}", strerror(errno));
    return;
  }
  spdlog::info("TCP4 listening on {}:{}", host, port);
}

void Tcp4Layer::SetupNonBlocking(int s) {
  int flags = fcntl(s, F_GETFL);
  if (flags < 0) {
    spdlog::error("TCP4 fcntl(): {}", strerror(errno));
    return;
  }
  flags = fcntl(s, F_SETFL, flags | O_NONBLOCK);
  if (flags < 0) {
    spdlog::error("TCP4 fcntl(): {}", strerror(errno));
    return;
  }
}

void Tcp4Layer::StartEpoll() {
  epollDescriptor = epoll_create1(0);
  if (epollDescriptor < 0) {
    spdlog::error("TCP4 epoll_create1(): {}", strerror(errno));
    return;
  }

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = localDescriptor;
  int r = epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, localDescriptor, &event);
  if (r < 0) {
    spdlog::error("TCP4 epoll_ctl(): {}", strerror(errno));
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

void Tcp4Layer::SetupPeer() {
  sockaddr_in peerAddr;
  unsigned int n = sizeof peerAddr;
  bzero(&peerAddr, n);
  int s = accept(localDescriptor, reinterpret_cast<sockaddr*>(&peerAddr), &n);
  if (s < 0) {
    spdlog::error("TCP4 accept(): {}", strerror(errno));
    return;
  }
  SetupNonBlocking(s);

  epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = s;
  epoll_ctl(epollDescriptor, EPOLL_CTL_ADD, s, &event);
}

void Tcp4Layer::ReadFromPeer(int peer) {
  auto sender = std::make_unique<TcpSender>(peer, *this);
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

void Tcp4Layer::OnPeerClose(int peerDescriptor) {
  if (peerDescriptor > 0) {
    int r = epoll_ctl(epollDescriptor, EPOLL_CTL_DEL, peerDescriptor, nullptr);
    if (r < 0) {
      spdlog::error("TCP4 epoll_ctl(): {}", strerror(errno));
      return;
    }
  }
}

TcpSender::TcpSender(int s, NetworkSupervisor& supervisor) : peerDescriptor{s}, supervisor{supervisor} {
  int flag = 0;
  int r = setsockopt(peerDescriptor, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
  if (r < 0) {
    spdlog::error("TCP4 setsockopt(): {}", strerror(errno));
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
    supervisor.OnPeerClose(peerDescriptor);
    close(peerDescriptor);
    peerDescriptor = -1;
  }
}

}  // namespace network
