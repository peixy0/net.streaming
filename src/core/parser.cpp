#include "parser.hpp"
#include "common.hpp"

namespace network {

std::optional<HttpRequest> ConcreteHttpParser::Parse(std::string& payload_) const {
  std::string payload = payload_;
  auto method = ParseToken(payload);
  if (not method) {
    return std::nullopt;
  }
  common::ToLower(*method);
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

std::optional<WebsocketFrame> ConcreteWebsocketFrameParser::Parse(std::string& payload) const {
  int payloadLen = payload.length();
  int requiredLen = headerLen;
  if (payloadLen < requiredLen) {
    return std::nullopt;
  }
  WebsocketFrame frame;
  const auto* p = reinterpret_cast<const unsigned char*>(payload.data());
  frame.fin = (p[0] >> 7) & 0b1;
  frame.opcode = p[0] & 0b1111;
  bool mask = (p[1] >> 7) & 0b1;
  std::uint64_t len = p[1] & 0b1111111;
  p += headerLen;
  int payloadExtLen = 0;
  if (len == 126) {
    payloadExtLen = ext1Len;
  }
  if (len == 127) {
    payloadExtLen = ext2Len;
  }
  requiredLen += payloadExtLen;
  if (payloadLen < requiredLen) {
    return std::nullopt;
  }
  if (payloadExtLen > 0) {
    len = 0;
    for (int i = 0; i < payloadExtLen; i++) {
      len |= p[i] << (8 * (payloadExtLen - i - 1));
    }
    p += payloadExtLen;
  }
  unsigned char maskKey[maskLen] = {0};
  if (mask) {
    requiredLen += maskLen;
    if (payloadLen < requiredLen) {
      return std::nullopt;
    }
    for (int i = 0; i < maskLen; i++) {
      maskKey[i] = p[i];
    }
    p += maskLen;
  }
  requiredLen += len;
  if (payloadLen < requiredLen) {
    return std::nullopt;
  }
  std::string data{p, p + len};
  for (std::uint64_t i = 0; i < len; i++) {
    data[i] ^= reinterpret_cast<const std::uint8_t*>(&maskKey)[i % maskLen];
  }
  frame.payload = std::move(data);
  payload.erase(0, requiredLen);
  return frame;
}

}  // namespace network
