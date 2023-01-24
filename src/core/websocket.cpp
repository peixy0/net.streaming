#include "websocket.hpp"
#include <spdlog/spdlog.h>
#include "common.hpp"

namespace network {

ConcreteWebsocketSender::ConcreteWebsocketSender(TcpSender& sender) : sender{sender} {
}

void ConcreteWebsocketSender::Send(WebsocketFrame&& frame) {
  std::string payload;
  payload += static_cast<unsigned char>((frame.fin << 7) | (frame.opcode));
  std::uint64_t payloadLen = frame.payload.length();
  if (payloadLen < 126) {
    payload += static_cast<unsigned char>(payloadLen);
  } else if (payloadLen < (1 << 16)) {
    payload += static_cast<unsigned char>(126);
    payload += static_cast<unsigned char>((payloadLen >> 8) & 0xff);
    payload += static_cast<unsigned char>(payloadLen & 0xff);
  } else {
    payload += static_cast<unsigned char>(127);
    payload += static_cast<unsigned char>((payloadLen >> 24) & 0xff);
    payload += static_cast<unsigned char>((payloadLen >> 16) & 0xff);
    payload += static_cast<unsigned char>((payloadLen >> 8) & 0xff);
    payload += static_cast<unsigned char>(payloadLen & 0xff);
  }
  payload += frame.payload;
  sender.Send(payload);
}

void ConcreteWebsocketSender::Close() {
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
  network::HttpResponse resp;
  resp.status = network::HttpStatus::SwitchingProtocols;
  resp.headers.emplace("Upgrade", "websocket");
  resp.headers.emplace("Connection", "Upgrade");
  resp.headers.emplace("Sec-WebSocket-Accept", std::move(accept));
  return resp;
}

WebsocketLayer::WebsocketLayer(std::unique_ptr<network::WebsocketFrameParser> parser,
    std::unique_ptr<network::WebsocketFrameSender> sender_, WebsocketProcessorFactory& processorFactory)
    : parser{std::move(parser)}, sender{std::move(sender_)}, processor{processorFactory.Create(*sender)} {
}

WebsocketLayer::~WebsocketLayer() {
  processor.reset();
  parser.reset();
  sender.reset();
}

void WebsocketLayer::Process(std::string& payload) {
  auto frame = parser->Parse(payload);
  if (not frame) {
    return;
  }
  spdlog::debug("websocket received frame: fin = {}, opcode = {}", frame->fin, frame->opcode);
  spdlog::debug("websocket received message: {}", frame->payload);
  if (frame->opcode == opClose) {
    sender->Close();
    return;
  }
  processor->Process(std::move(*frame));
}

ConcreteWebsocketLayerFactory::ConcreteWebsocketLayerFactory(
    std::unique_ptr<WebsocketProcessorFactory> processorFactory)
    : processorFactory{std::move(processorFactory)} {
}

std::unique_ptr<ProtocolProcessor> ConcreteWebsocketLayerFactory::Create(TcpSender& tcpSender) const {
  auto parser = std::make_unique<ConcreteWebsocketFrameParser>();
  auto sender = std::make_unique<ConcreteWebsocketSender>(tcpSender);
  return std::make_unique<WebsocketLayer>(std::move(parser), std::move(sender), *processorFactory);
}

}  // namespace network
