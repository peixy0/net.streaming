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
    case network::HttpStatus::BadRequest:
      return "400 Bad Request";
    case network::HttpStatus::NotFound:
      return "404 Not Found";
  }
  return "";
}

}  // namespace

namespace network {

HttpResponseVisitor::HttpResponseVisitor(TcpSender& sender) : sender{sender} {
}

void HttpResponseVisitor::operator()(PreparedHttpResponse&& response) const {
  std::string respPayload = "HTTP/1.1 " + to_string(response.status) + "\r\n";
  response.headers.emplace("Content-Length", std::to_string(response.body.length()));
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  respPayload += std::move(response.body);
  sender.Send(std::move(respPayload));
}

void HttpResponseVisitor::operator()(FileHttpResponse&& response) const {
  os::File file{response.path};
  if (not file.Ok()) {
    spdlog::error("http open(\"{}\"): {}", response.path, strerror(errno));
    PreparedHttpResponse resp;
    resp.status = HttpStatus::NotFound;
    resp.headers.emplace("Content-Type", "text/plain");
    resp.body = "Not Found";
    return operator()(std::move(resp));
  }
  std::string respPayload = "HTTP/1.1 " + to_string(HttpStatus::OK) + "\r\n";
  response.headers.emplace("Content-Length", std::to_string(file.Size()));
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  sender.Send(std::move(respPayload));
  sender.Send(std::move(file));
}

void HttpResponseVisitor::operator()(RawStreamHttpResponse&& response) const {
  auto stream = response.streamFactory->GetStream(sender);
  sender.Send(std::move(stream));
}

HttpLayer::HttpLayer(const HttpOptions& options, std::unique_ptr<HttpParser> parser, HttpProcessor& processor,
                     TcpSender& sender)
    : options{options}, parser{std::move(parser)}, processor{processor}, sender{sender} {
}

void HttpLayer::Receive(std::string_view payload) {
  parser->Append(payload);
  size_t receivedPayloadSize = parser->GetLength();
  if (receivedPayloadSize > options.maxPayloadSize) {
    spdlog::error("http received payload exceeds limit");
    sender.Close();
    return;
  }
  spdlog::debug("http received payload: {}", payload);
  auto request = parser->Parse();
  if (not request) {
    return;
  }
  auto response = processor.Process(std::move(*request));
  std::visit(HttpResponseVisitor{sender}, std::move(response));
}

HttpLayerFactory::HttpLayerFactory(const HttpOptions& options, HttpProcessor& processor)
    : options{options}, processor{processor} {
}

std::unique_ptr<network::TcpReceiver> HttpLayerFactory::Create(TcpSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(options, std::move(parser), processor, sender);
}

}  // namespace network
