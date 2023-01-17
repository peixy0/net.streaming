#include "parser.hpp"

namespace {

void ToLower(std::string& s) {
  for (char& c : s) {
    c = tolower(c);
  }
}

}  // namespace

namespace network {

std::optional<HttpRequest> ConcreteHttpParser::Parse() {
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
    uriBase = ParseUriBase(*uri);
    query = ParseQueryString(*uri);
  }
  if (not version) {
    version = ParseToken(payload);
    if (not version) {
      return std::nullopt;
    }
  }
  if (not requestLineEndingParsed) {
    requestLineEndingParsed = Consume(payload, "\r\n");
    if (not requestLineEndingParsed) {
      return std::nullopt;
    }
  }
  if (not headersParsed) {
    headersParsed = ParseHeaders(payload, headers);
    if (not headersParsed) {
      return std::nullopt;
    }
  }
  if (not headersEndingParsed) {
    headersEndingParsed = Consume(payload, "\r\n");
    if (not headersEndingParsed) {
      return std::nullopt;
    }
    bodyRemaining = FindContentLength(headers);
  }
  if (payload.length() < bodyRemaining) {
    return std::nullopt;
  }
  std::string body = payload.substr(0, bodyRemaining);
  payload.erase(0, bodyRemaining);
  HttpRequest request{std::move(*method), std::move(uriBase), std::move(*version), std::move(headers), std::move(query),
      std::move(body)};
  Reset();
  return request;
}

void ConcreteHttpParser::Append(std::string_view received) {
  receivedLength += received.length();
  payload += received;
}

size_t ConcreteHttpParser::GetLength() const {
  return receivedLength;
}

void ConcreteHttpParser::Reset() {
  receivedLength = payload.length();
  method.reset();
  uri.reset();
  version.reset();
  requestLineEndingParsed = false;
  headers.clear();
  headersParsed = false;
  headersEndingParsed = false;
  bodyRemaining = 0;
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
  ToLower(*field);
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

}  // namespace network
