#include "websocket.hpp"
#include <spdlog/spdlog.h>
#include "common.hpp"

namespace network {

std::optional<WebsocketFrame> ConcreteWebsocketParser::Parse(std::string& payload) const {
  std::uint64_t payloadLen = payload.length();
  std::uint64_t requiredLen = headerLen;
  if (payloadLen < requiredLen) {
    return std::nullopt;
  }
  WebsocketFrame frame;
  const auto* p = reinterpret_cast<const std::uint8_t*>(payload.data());
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
  std::uint8_t maskKey[maskLen] = {0};
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
    reinterpret_cast<std::uint8_t&>(data[i]) ^= maskKey[i % maskLen];
  }
  frame.payload = std::move(data);
  payload.erase(0, requiredLen);
  return frame;
}

ConcreteWebsocketSender::ConcreteWebsocketSender(TcpSender& sender) : sender{sender} {
}

void ConcreteWebsocketSender::Send(WebsocketFrame&& frame) const {
  std::string payload;
  payload += common::ToChar((frame.fin << 7) | (frame.opcode));
  std::uint64_t payloadLen = frame.payload.length();
  if (payloadLen < 126) {
    payload += common::ToChar(payloadLen);
  } else if (payloadLen < (1 << 16)) {
    payload += common::ToChar(126);
    payload += common::ToChar(payloadLen >> 8);
    payload += common::ToChar(payloadLen);
  } else {
    payload += common::ToChar(127);
    payload += common::ToChar(payloadLen >> 24);
    payload += common::ToChar(payloadLen >> 16);
    payload += common::ToChar(payloadLen >> 8);
    payload += common::ToChar(payloadLen);
  }
  payload += frame.payload;
  sender.Send(payload);
}

void ConcreteWebsocketSender::Close() const {
  sender.Close();
}

WebsocketHandshakeBuilder::WebsocketHandshakeBuilder(const HttpRequest& request) : request{request} {
}

std::optional<HttpResponse> WebsocketHandshakeBuilder::Build() const {
  auto upgradeIt = request.headers.find("upgrade");
  if (upgradeIt == request.headers.end()) {
    return std::nullopt;
  }
  auto upgrade = upgradeIt->second;
  common::ToLower(upgrade);
  if (upgrade != "websocket") {
    return std::nullopt;
  }
  auto keyIt = request.headers.find("sec-websocket-key");
  if (keyIt == request.headers.end()) {
    return std::nullopt;
  }
  auto accept = common::Base64(common::SHA1(keyIt->second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
  HttpResponse resp;
  resp.status = HttpStatus::SwitchingProtocols;
  resp.headers.emplace("Upgrade", "websocket");
  resp.headers.emplace("Connection", "Upgrade");
  resp.headers.emplace("Sec-WebSocket-Accept", std::move(accept));
  return resp;
}

WebsocketLayer::WebsocketLayer(WebsocketParser& parser, WebsocketSender& sender, WebsocketProcessor& processor)
    : parser{parser}, sender{sender}, processor{processor} {
}

bool WebsocketLayer::TryProcess(std::string& payload) const {
  auto frame = parser.Parse(payload);
  if (not frame) {
    return false;
  }
  spdlog::debug("websocket received frame: fin = {}, opcode = {}", frame->fin, frame->opcode);
  spdlog::debug("websocket received message: {}", frame->payload);
  constexpr int opClose{8};
  if (frame->opcode == opClose) {
    sender.Close();
    return true;
  }
  processor.Process(std::move(*frame));
  return true;
}

}  // namespace network
