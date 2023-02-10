#pragma once
#include <regex>
#include <string>
#include <vector>
#include "http.hpp"
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

class ConcreteRouter final : public Router {
public:
  ConcreteRouter(HttpRouteMapping& httpMapping, WebsocketRouteMapping& websocketMapping, TcpSender& sender,
      ProtocolDispatcher& dispatcher)
      : httpMapping{httpMapping},
        httpSender{sender},
        httpLayer{httpParser, httpSender, *this},
        websocketMapping{websocketMapping},
        websocketSender{sender},
        websocketLayer{websocketParser, websocketSender, *this},
        dispatcher{dispatcher} {
    dispatcher.SetProcessor(&httpLayer);
  }

  void Process(HttpRequest&& req) override;
  void Process(WebsocketFrame&& req) override;

private:
  bool TryUpgrade(const HttpRequest& req);

  HttpRouteMapping& httpMapping;
  ConcreteHttpSender httpSender;
  ConcreteHttpParser httpParser;
  HttpLayer httpLayer;
  WebsocketRouteMapping& websocketMapping;
  ConcreteWebsocketSender websocketSender;
  ConcreteWebsocketParser websocketParser;
  WebsocketLayer websocketLayer;
  ProtocolDispatcher& dispatcher;
  std::unique_ptr<HttpProcessor> httpProcessor;
  std::unique_ptr<WebsocketProcessor> websocketProcessor;
};

class ConcreteRouterFactory final : public RouterFactory {
public:
  ConcreteRouterFactory(HttpRouteMapping& httpMapping, WebsocketRouteMapping& websocketMapping)
      : httpMapping{httpMapping}, websocketMapping{websocketMapping} {
  }

  std::unique_ptr<Router> Create(TcpSender& sender, ProtocolDispatcher& dispatcher) const override {
    return std::make_unique<ConcreteRouter>(httpMapping, websocketMapping, sender, dispatcher);
  }

private:
  HttpRouteMapping& httpMapping;
  WebsocketRouteMapping& websocketMapping;
};

}  // namespace network
