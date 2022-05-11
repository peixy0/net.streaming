#include "http.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include "file.hpp"
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
  std::string respPayload = "HTTP/1.1 " + to_string(response.status) + "\r\n";
  respPayload += "content-type: text/plain;charset=utf-8\r\n";
  respPayload += "content-length: " + std::to_string(response.body.length()) + "\r\n\r\n";
  respPayload += response.body;
  sender.Send(std::move(respPayload));
}

void HttpResponseVisitor::operator()(const FileHttpResponse& response) const {
  os::File file{response.path};
  if (not file.Ok()) {
    spdlog::error("http open(\"{}\"): {}", response.path, strerror(errno));
    std::string respPayload = "HTTP/1.1 " + to_string(HttpStatus::NotFound) + "\r\n";
    respPayload += "content-length: 0\r\n\r\n";
    sender.Send(std::move(respPayload));
    return;
  }
  std::string respPayload = "HTTP/1.1 " + to_string(HttpStatus::OK) + "\r\n";
  respPayload += "content-type: " + response.contentType + "\r\n";
  respPayload += "content-length: " + std::to_string(file.Size()) + "\r\n\r\n";
  sender.Send(std::move(respPayload));
  sender.SendFile(std::move(file));
}

HttpLayer::HttpLayer(const HttpOptions& options, std::unique_ptr<HttpParser> parser, HttpProcessor& processor,
                     TcpSender& sender)
    : options{options}, parser{std::move(parser)}, processor{processor}, sender{sender} {
}

void HttpLayer::Receive(std::string_view payload) {
  receivedPayloadSize += payload.size();
  if (receivedPayloadSize > options.maxPayloadSize) {
    spdlog::error("http received payload exceeds limit");
    sender.Close();
    return;
  }
  spdlog::debug("http received payload: {}", payload);
  receivedPayload.append(std::move(payload));
  auto request = parser->Parse(receivedPayload);
  if (not request) {
    return;
  }
  receivedPayloadSize = receivedPayload.size();
  auto response = processor.Process(std::move(*request));
  std::visit(HttpResponseVisitor{sender}, response);
}

HttpLayerFactory::HttpLayerFactory(const HttpOptions& options, HttpProcessor& processor)
    : options{options}, processor{processor} {
}

std::unique_ptr<network::TcpReceiver> HttpLayerFactory::Create(TcpSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(options, std::move(parser), processor, sender);
}

}  // namespace network
