#include "router.hpp"

namespace network {

bool ConcreteRouter::TryUpgrade(const HttpRequest& req) {
  auto* entry = websocketMapping.Get(req.uri);
  if (not entry) {
    return false;
  }
  WebsocketHandshakeBuilder handshake{req};
  auto resp = handshake.Build();
  if (not resp) {
    return false;
  }
  httpProcessor.reset();
  dispatcher.SetProcessor(&websocketLayer);
  websocketProcessor = entry->Create(websocketSender);
  httpSender.Send(std::move(*resp));
  return true;
}

void ConcreteRouter::Process(HttpRequest&& req) {
  if (TryUpgrade(req)) {
    return;
  }
  auto entry = httpMapping.Get(req.method, req.uri);
  if (entry) {
    websocketProcessor.reset();
    httpProcessor = entry->Create(httpSender);
    httpProcessor->Process(std::move(req));
    return;
  }
  HttpResponse resp;
  resp.status = HttpStatus::NotFound;
  httpSender.Send(std::move(resp));
}

void ConcreteRouter::Process(WebsocketFrame&& req) {
  if (not websocketProcessor) {
    websocketSender.Close();
  }
  websocketProcessor->Process(std::move(req));
}

}  // namespace network
