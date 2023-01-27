#pragma once
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include "network.hpp"
#include "websocket.hpp"

namespace network {

class HttpRouteMapping {
public:
  void Add(HttpMethod method, const std::string& uri, std::unique_ptr<HttpProcessorFactory> processorFactory) {
    mapping.emplace_back(std::make_tuple<HttpMethod, std::regex, std::unique_ptr<HttpProcessorFactory>>(
        std::move(method), std::regex{uri}, std::move(processorFactory)));
  }

  HttpProcessorFactory* Get(HttpMethod method, const std::string& uri) const {
    for (const auto& [m, k, v] : mapping) {
      if (m == method and std::regex_match(uri, k)) {
        return v.get();
      }
    }
    return nullptr;
  }

private:
  std::vector<std::tuple<HttpMethod, std::regex, std::unique_ptr<HttpProcessorFactory>>> mapping;
};

class WebsocketRouteMapping {
public:
  void Add(const std::string& uri, std::unique_ptr<WebsocketProcessorFactory> processorFactory) {
    mapping.emplace_back(std::make_tuple<std::regex, std::unique_ptr<WebsocketProcessorFactory>>(
        std::regex{uri}, std::move(processorFactory)));
  }

  WebsocketProcessorFactory* Get(const std::string& uri) const {
    for (const auto& [k, v] : mapping) {
      if (std::regex_match(uri, k)) {
        return v.get();
      }
    }
    return nullptr;
  }

private:
  std::vector<std::tuple<std::regex, std::unique_ptr<WebsocketProcessorFactory>>> mapping;
};

class ConcreteRouter : public Router {
public:
  ConcreteRouter(HttpSender& httpSender, HttpRouteMapping& httpMapping, WebsocketSender& websocketSender,
      WebsocketRouteMapping& websocketMapping, ProtocolUpgrader& upgrader)
      : httpSender{httpSender},
        httpMapping{httpMapping},
        websocketSender{websocketSender},
        websocketMapping{websocketMapping},
        upgrader{upgrader} {
  }

  bool TryUpgrade(const HttpRequest& req) {
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
    websocketProcessor = entry->Create(websocketSender);
    httpSender.Send(std::move(*resp));
    return true;
  }

  void Process(HttpRequest&& req) {
    if (TryUpgrade(req)) {
      upgrader.UpgradeToWebsocket();
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

  void Process(WebsocketFrame&& req) {
    if (not websocketProcessor) {
      websocketSender.Close();
    }
    websocketProcessor->Process(std::move(req));
  }

private:
  HttpSender& httpSender;
  HttpRouteMapping& httpMapping;
  WebsocketSender& websocketSender;
  WebsocketRouteMapping& websocketMapping;
  ProtocolUpgrader& upgrader;
  std::unique_ptr<HttpProcessor> httpProcessor;
  std::unique_ptr<WebsocketProcessor> websocketProcessor;
};

class ConcreteRouterFactory : public RouterFactory {
public:
  ConcreteRouterFactory(HttpRouteMapping& httpMapping, WebsocketRouteMapping& websocketMapping)
      : httpMapping{httpMapping}, websocketMapping{websocketMapping} {
  }

  std::unique_ptr<Router> Create(
      HttpSender& httpSender, WebsocketSender& websocketSender, ProtocolUpgrader& upgrader) const override {
    return std::make_unique<ConcreteRouter>(httpSender, httpMapping, websocketSender, websocketMapping, upgrader);
  }

private:
  HttpRouteMapping& httpMapping;
  WebsocketRouteMapping& websocketMapping;
};

}  // namespace network
