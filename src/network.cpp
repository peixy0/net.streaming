#include "network.hpp"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

TcpLayer::TcpLayer(std::string_view host, std::uint16_t port, NetworkLayerFactory& networkLayerFactory)
    : host{host}, port{port}, networkLayerFactory{networkLayerFactory} {
}

TcpLayer::~TcpLayer() {
  if (localDescriptor < 0) {
    return;
  }
  close(localDescriptor);
  localDescriptor = -1;
}

void TcpLayer::Start() {
  SetupSocket();
  while (true) {
    ReadFromPeer();
  }
}

void TcpLayer::SetupSocket() {
  localDescriptor = socket(AF_INET, SOCK_STREAM, 0);
  if (localDescriptor < 0) {
    perror("TCP socket()");
    return;
  }
  int flag = 1;
  setsockopt(localDescriptor, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag);

  sockaddr_in localAddr;
  bzero(&localAddr, sizeof localAddr);
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = inet_addr(host.c_str());
  localAddr.sin_port = htons(port);
  int rc = bind(localDescriptor, reinterpret_cast<sockaddr*>(&localAddr), sizeof localAddr);
  if (rc != 0) {
    perror("TCP bind()");
    return;
  }

  rc = listen(localDescriptor, 0);
  if (rc < 0) {
    perror("TCP listen()");
    return;
  }
}

void TcpLayer::ReadFromPeer() {
  sockaddr_in peerAddr;
  unsigned int n = sizeof peerAddr;
  bzero(&peerAddr, n);
  int s = accept(localDescriptor, reinterpret_cast<sockaddr*>(&peerAddr), &n);
  if (s < 0) {
    perror("TCP accept()");
    return;
  }
  auto sender = std::make_unique<TcpSender>(s);
  auto upperLayer = networkLayerFactory.Create(*sender);
  char buf[512];
  int size = sizeof buf;
  int r = 0;
  do {
    r = recv(s, buf, size, 0);
    if (r < 0) {
      return;
    }
    upperLayer->Receive({buf, buf + r});
  } while (r > 0);
}

TcpSender::TcpSender(int s) : peerDescriptor{s} {
  int flag = 0;
  setsockopt(peerDescriptor, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
}

TcpSender::~TcpSender() {
  Close();
}

void TcpSender::Send(std::string_view buf) {
  const std::string& s{buf.cbegin(), buf.cend()};
  send(peerDescriptor, s.c_str(), s.length(), 0);
}

void TcpSender::Close() {
  if (peerDescriptor < 0) {
    return;
  }
  close(peerDescriptor);
  peerDescriptor = -1;
}

}  // namespace network
