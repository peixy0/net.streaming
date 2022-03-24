#include "http.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include "network.hpp"

namespace network {

std::optional<HttpRequest> ConcreteHttpParser::Parse(std::string& payload) {
  if (not method) {
    method = ParseToken(payload);
    if (not method) {
      return std::nullopt;
    }
    ToLower(*method);
  }
  if (not uri) {
    uri = ParseToken(payload);
    if (not uri) {
      return std::nullopt;
    }
  }
  if (not version) {
    version = ParseToken(payload);
    if (not version) {
      return std::nullopt;
    }
    if (not Consume(payload, "\r\n")) {
      return std::nullopt;
    }
  }
  if (not headerParsed) {
    headerParsed = ParseHeaders(payload, headers);
    if (not headerParsed) {
      return std::nullopt;
    }
    if (not Consume(payload, "\r\n")) {
      return std::nullopt;
    }
  }
  bodyRemaining = ParseContentLength(headers);
  if (payload.length() < bodyRemaining) {
    return std::nullopt;
  }
  std::string body = payload.substr(0, bodyRemaining);
  payload.erase(0, bodyRemaining);
  HttpRequest request{std::move(*method), std::move(*uri), std::move(*version), std::move(headers), std::move(body)};
  Reset();
  return request;
}

void ConcreteHttpParser::Reset() {
  method.reset();
  uri.reset();
  version.reset();
  headerParsed = false;
  headers.clear();
  bodyRemaining = 0;
}

void ConcreteHttpParser::ToLower(std::string& token) const {
  for (auto& c : token) {
    c = std::tolower(c);
  }
}

void ConcreteHttpParser::SkipWhiteSpaces(std::string& payload) const {
  while (not payload.empty() and payload[0] == ' ') {
    payload.erase(0, 1);
  }
}

bool ConcreteHttpParser::Consume(std::string& payload, std::string_view value) const {
  int payloadLength = payload.length();
  int valueLength = value.length();
  for (int i = 0; i < valueLength; i++) {
    if (i >= payloadLength or payload[i] != value[i]) {
      return false;
    }
  }
  payload.erase(0, valueLength);
  return true;
}

std::optional<std::string> ConcreteHttpParser::ParseToken(std::string& payload) const {
  SkipWhiteSpaces(payload);
  if (payload.empty()) {
    return std::nullopt;
  }
  int n = 0;
  int length = payload.length();
  while (n < length and payload[n] != ' ' and payload[n] != '\r' and payload[n] != '\n') {
    n++;
  }
  if (n == length) {
    return std::nullopt;
  }
  std::string token = payload.substr(0, n);
  payload.erase(0, n);
  return token;
}

bool ConcreteHttpParser::ParseHeaders(std::string& payload, HttpHeaders& headers) const {
  while (not payload.empty()) {
    if (payload.starts_with("\r\n")) {
      return true;
    }
    auto header = ParseHeader(payload);
    if (not header) {
      return false;
    }
    headers.emplace(std::move(header->field), std::move(header->value));
  }
  return false;
}

std::optional<HttpHeader> ConcreteHttpParser::ParseHeader(std::string& payload) const {
  auto line = ParseLine(payload);
  if (not line) {
    return std::nullopt;
  }
  ToLower(*line);
  int length = line->length();
  int n = 0;
  while (n < length and (*line)[n] != ':') {
    n++;
  }
  if (n == length) {
    return HttpHeader{std::move(*line), ""};
  }
  auto field = line->substr(0, n);
  line->erase(0, n + 1);
  SkipWhiteSpaces(*line);
  return HttpHeader{std::move(field), std::move(*line)};
}

std::optional<std::string> ConcreteHttpParser::ParseLine(std::string& payload) const {
  int length = payload.length();
  int n = 0;
  while (n + 1 < length and (payload[n] != '\r' or payload[n + 1] != '\n')) {
    n++;
  }
  n += 2;
  if (n >= length) {
    return std::nullopt;
  }
  std::string s = payload.substr(0, n - 2);
  payload.erase(0, n);
  return s;
}

std::uint32_t ConcreteHttpParser::ParseContentLength(const HttpHeaders& headers) const {
  auto it = headers.find("content-length");
  if (it != headers.end()) {
    std::uint32_t r = stoul(it->second);
    return r > 0 ? r : 0;
  }
  return 0;
}

HttpLayer::HttpLayer(std::unique_ptr<HttpParser> parser, HttpProcessor& processor, NetworkSender& sender)
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
  receivedPayload.append(packet);
  auto request = parser->Parse(receivedPayload);
  if (not request) {
    return;
  }
  payloadSize = receivedPayload.size();
  if (request->uri != "/") {
    sender.Send("HTTP/1.1 404 Not Found\r\n\r\n");
    return;
  }
  auto response = processor.Process(*request);
  sender.Send(
      "HTTP/1.1 200 OK\r\n"
      "content-length: " +
      std::to_string(response.body.length()) + "\r\n\r\n" + response.body);
}

HttpLayerFactory::HttpLayerFactory(HttpProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::NetworkLayer> HttpLayerFactory::Create(NetworkSender& sender) const {
  auto parser = std::make_unique<ConcreteHttpParser>();
  return std::make_unique<HttpLayer>(std::move(parser), processor, sender);
}

}  // namespace network
