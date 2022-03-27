#include "http.hpp"
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <cctype>
#include "network.hpp"

namespace {

std::string to_string(network::HttpStatus status) {
  switch (status) {
    case network::HttpStatus::OK:
      return "200 OK";
    case network::HttpStatus::NotFound:
      return "404 Not Found";
  }
  return "";
}

}  // namespace

namespace network {

HttpResponseVisitor::HttpResponseVisitor(TcpSender& sender) : sender{sender} {
}

void HttpResponseVisitor::operator()(const PlainTextHttpResponse& response) const {
  std::string respPacket = "HTTP/1.1 " + to_string(response.status) + "\r\n";
  respPacket += "content-type: text/plain;charset=utf-8\r\n";
  respPacket += "content-length: " + std::to_string(response.body.length()) + "\r\n\r\n";
  respPacket += response.body;
  sender.Send(std::move(respPacket));
}

void HttpResponseVisitor::operator()(const FileHttpResponse& response) const {
  int fd = open(response.path.c_str(), O_RDONLY);
  if (fd < 0) {
    spdlog::error("http open(): {}", strerror(errno));
    sender.Send("HTTP/1.1" + to_string(HttpStatus::NotFound) + "\r\n\r\n");
    return;
  }
  struct stat statbuf;
  fstat(fd, &statbuf);
  size_t size = statbuf.st_size;
  std::string respPacket = "HTTP/1.1" + to_string(HttpStatus::OK) + "\r\n";
  respPacket += "content-type: " + response.contentType + "\r\n";
  respPacket += "content-length: " + std::to_string(size) + "\r\n\r\n";
  sender.Send(respPacket);
  sender.SendFile(fd, size);
  close(fd);
}

HttpLayer::HttpLayer(std::unique_ptr<HttpParser> parser, HttpProcessor& processor, TcpSender& sender)
    : parser{std::move(parser)}, processor{processor}, sender{sender} {
}

void HttpLayer::Receive(std::string_view packet) {
  payloadSize += packet.size();
  if (payloadSize > 1 << 20) {
    spdlog::error("http received payload exceeds limit");
    sender.Close();
    return;
  }
  spdlog::debug("http received packet: {}", packet);
  receivedPayload.append(std::move(packet));
  auto request = parser->Parse(receivedPayload);
  if (not request) {
    return;
  }
  payloadSize = receivedPayload.size();
  auto response = processor.Process(std::move(*request));
  std::visit(HttpResponseVisitor{sender}, response);
}

HttpLayerFactory::HttpLayerFactory(HttpProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::TcpReceiver> HttpLayerFactory::Create(TcpSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(std::move(parser), processor, sender);
}

}  // namespace network
