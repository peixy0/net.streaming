#include "http.hpp"
#include <spdlog/spdlog.h>
#include <cctype>
#include <sstream>
#include "common.hpp"
#include "file.hpp"
#include "network.hpp"

namespace {

std::string ToString(network::HttpStatus status) {
  switch (status) {
    case network::HttpStatus::SwitchingProtocols:
      return "101 Switching Protocols";
    case network::HttpStatus::OK:
      return "200 OK";
    case network::HttpStatus::BadRequest:
      return "400 Bad Request";
    case network::HttpStatus::NotFound:
      return "404 Not Found";
  }
  return "";
}

std::string ToString(network::HttpMethod method) {
  switch (method) {
    case network::HttpMethod::GET:
      return "GET";
    case network::HttpMethod::PUT:
      return "PUT";
    case network::HttpMethod::POST:
      return "POST";
    case network::HttpMethod::DELETE:
      return "DELETE";
  }
  return "";
}

std::optional<network::HttpMethod> ConvertMethod(std::string_view method) {
  if (method == "get") {
    return network::HttpMethod::GET;
  }
  if (method == "put") {
    return network::HttpMethod::PUT;
  }
  if (method == "post") {
    return network::HttpMethod::POST;
  }
  if (method == "delete") {
    return network::HttpMethod::DELETE;
  }
  return std::nullopt;
}

}  // namespace

namespace network {

std::optional<HttpRequest> ConcreteHttpParser::Parse(std::string& payload_) const {
  std::string payload = payload_;
  auto methodStr = ParseToken(payload);
  if (not methodStr) {
    return std::nullopt;
  }
  common::ToLower(*methodStr);
  auto method = ConvertMethod(*methodStr);
  if (not method) {
    return std::nullopt;
  }
  auto uri = ParseToken(payload);
  if (not uri) {
    return std::nullopt;
  }
  auto uriBase = ParseUriBase(*uri);
  auto query = ParseQueryString(*uri);
  auto version = ParseToken(payload);
  if (not version) {
    return std::nullopt;
  }
  auto requestLineEndingParsed = Consume(payload, "\r\n");
  if (not requestLineEndingParsed) {
    return std::nullopt;
  }
  HttpHeaders headers;
  auto headersParsed = ParseHeaders(payload, headers);
  if (not headersParsed) {
    return std::nullopt;
  }
  auto headersEndingParsed = Consume(payload, "\r\n");
  if (not headersEndingParsed) {
    return std::nullopt;
  }
  auto bodyRemaining = FindContentLength(headers);
  if (payload.length() < bodyRemaining) {
    return std::nullopt;
  }
  std::string body = payload.substr(0, bodyRemaining);
  payload.erase(0, bodyRemaining);
  HttpRequest request{std::move(*method), std::move(uriBase), std::move(*version), std::move(headers), std::move(query),
      std::move(body)};
  payload_ = payload;
  return request;
}

void ConcreteHttpParser::SkipWhiteSpaces(std::string& payload) const {
  size_t n = payload.find_first_not_of(' ');
  payload.erase(0, n);
}

bool ConcreteHttpParser::Consume(std::string& payload, std::string_view value) const {
  size_t payloadLength = payload.length();
  size_t valueLength = value.length();
  for (size_t i = 0; i < valueLength; i++) {
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
  const auto n = payload.find_first_of(" \r\n");
  if (n == payload.npos) {
    return std::nullopt;
  }
  const auto token = payload.substr(0, n);
  payload.erase(0, n);
  SkipWhiteSpaces(payload);
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
  auto field = ParseHeaderField(*line);
  if (not field) {
    return std::nullopt;
  }
  common::ToLower(*field);
  return HttpHeader{std::move(*field), std::move(*line)};
}

std::optional<std::string> ConcreteHttpParser::ParseHeaderField(std::string& payload) const {
  SkipWhiteSpaces(payload);
  const auto n = payload.find(':');
  if (n == payload.npos) {
    return std::nullopt;
  }
  const auto s = payload.substr(0, n);
  payload.erase(0, n + 1);
  SkipWhiteSpaces(payload);
  return s;
}

std::optional<std::string> ConcreteHttpParser::ParseLine(std::string& payload) const {
  const auto n = payload.find("\r\n");
  if (n == payload.npos) {
    return std::nullopt;
  }
  const auto s = payload.substr(0, n);
  payload.erase(0, n + 2);
  return s;
}

size_t ConcreteHttpParser::FindContentLength(const HttpHeaders& headers) const {
  const auto it = headers.find("content-length");
  if (it != headers.end()) {
    size_t r = stoul(it->second);
    return r > 0 ? r : 0;
  }
  return 0;
}

std::string ConcreteHttpParser::ParseUriBase(std::string& uri) const {
  const auto n = uri.find('?');
  const auto r = uri.substr(0, n);
  uri.erase(0, n);
  return r;
}

std::string ConcreteHttpParser::ParseQueryKey(std::string& uri) const {
  const auto n = uri.find('=');
  const auto r = uri.substr(0, n);
  uri.erase(0, n);
  return r;
}

std::string ConcreteHttpParser::ParseQueryValue(std::string& uri) const {
  const auto n = uri.find('&');
  const auto r = uri.substr(0, n);
  uri.erase(0, n);
  return r;
}

HttpQuery ConcreteHttpParser::ParseQueryString(std::string& uri) const {
  HttpQuery result;
  if (uri.empty() or not Consume(uri, "?")) {
    return result;
  }
  while (true) {
    auto key = ParseQueryKey(uri);
    if (not Consume(uri, "=")) {
      result.emplace(std::move(key), "");
      break;
    }
    auto value = ParseQueryValue(uri);
    result.emplace(std::move(key), std::move(value));
    if (not Consume(uri, "&")) {
      break;
    }
  }
  return result;
}

ConcreteHttpSender::ConcreteHttpSender(TcpSender& sender) : sender{sender} {
}

void ConcreteHttpSender::Send(HttpResponse&& response) const {
  std::string respPayload = "HTTP/1.1 " + ToString(response.status) + "\r\n";
  response.headers.emplace("Content-Length", std::to_string(response.body.length()));
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  respPayload += std::move(response.body);
  sender.Send(std::move(respPayload));
}

void ConcreteHttpSender::Send(FileHttpResponse&& response) const {
  os::File file{response.path};
  if (not file.Ok()) {
    spdlog::error("http open(\"{}\"): {}", response.path, strerror(errno));
    HttpResponse resp;
    resp.status = HttpStatus::NotFound;
    return Send(std::move(resp));
  }
  std::string respPayload = "HTTP/1.1 " + ToString(HttpStatus::OK) + "\r\n";
  response.headers.emplace("Content-Length", std::to_string(file.Size()));
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  sender.Send(std::move(respPayload));
  sender.Send(std::move(file));
}

void ConcreteHttpSender::Send(MixedReplaceHeaderHttpResponse&&) const {
  std::string respPayload = "HTTP/1.1 " + ToString(HttpStatus::OK) +
                            "\r\n"
                            "Content-Type: multipart/x-mixed-replace; boundary=\"BND\"\r\n\r\n";
  sender.Send(std::move(respPayload));
}

void ConcreteHttpSender::Send(MixedReplaceDataHttpResponse&& response) const {
  std::string respPayload = "--BND\r\n";
  response.headers.emplace("Content-Length", std::to_string(response.body.size()));
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  respPayload += std::move(response.body);
  respPayload += "\r\n";
  sender.Send(std::move(respPayload));
}

void ConcreteHttpSender::Send(ChunkedHeaderHttpResponse&& response) const {
  std::string respPayload = "HTTP/1.1 " + ToString(HttpStatus::OK) +
                            "\r\n"
                            "Transfer-Encoding: chunked\r\n";
  for (const auto& [k, v] : response.headers) {
    respPayload += k + ": " + v + "\r\n";
  }
  respPayload += "\r\n";
  sender.Send(std::move(respPayload));
}

void ConcreteHttpSender::Send(ChunkedDataHttpResponse&& response) const {
  std::stringstream ss;
  ss << std::hex << response.body.size();
  ss << "\r\n" << response.body << "\r\n";
  sender.Send(ss.str());
}

void ConcreteHttpSender::Close() const {
  sender.Close();
}

HttpLayer::HttpLayer(HttpParser& parser, HttpSender& sender_, HttpProcessor& processor)
    : parser{parser}, sender{sender_}, processor{processor} {
}

bool HttpLayer::TryProcess(std::string& payload) const {
  auto request = parser.Parse(payload);
  if (not request) {
    return false;
  }
  spdlog::debug("http layer received request: method = {}, uri = {}", ToString(request->method), request->uri);
  processor.Process(std::move(*request));
  return true;
}

}  // namespace network
