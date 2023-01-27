#include "server.hpp"
#include <functional>
#include "network.hpp"
#include "protocol.hpp"
#include "tcp.hpp"

namespace {

template <typename ProcessorT, typename RequestT, typename SenderT>
struct LambdaProcessorWrapper : public ProcessorT {
  LambdaProcessorWrapper(SenderT& sender, std::function<void(RequestT&&, SenderT&)> f) : sender{sender}, f{f} {
  }

  void Process(RequestT&& req) override {
    f(std::move(req), sender);
  }

  SenderT& sender;
  std::function<void(RequestT&&, SenderT&)> f;
};

template <typename ProcessorFactoryT, typename ProcessorT, typename RequestT, typename SenderT>
class LambdaProcessorFactoryWrapper : public ProcessorFactoryT {
public:
  LambdaProcessorFactoryWrapper(std::function<void(RequestT&&, SenderT&)> f) : f{f} {
  }

  std::unique_ptr<ProcessorT> Create(SenderT& sender) const override {
    return std::make_unique<LambdaProcessorWrapper<ProcessorT, RequestT, SenderT>>(sender, f);
  }

private:
  std::function<void(RequestT&&, SenderT&)> f;
};

}  // namespace

namespace network {

void Server::Start(std::string_view host, std::uint16_t port) {
  auto routerFactory = std::make_unique<ConcreteRouterFactory>(httpMapping, websocketMapping);
  auto protocolLayerFactory = std::make_unique<ProtocolLayerFactory>(*routerFactory);
  Tcp4Layer tcp{host, port, std::move(protocolLayerFactory)};
  tcp.Start();
}

void Server::Add(HttpMethod method, const std::string& uri, std::unique_ptr<HttpProcessorFactory> processorFactory) {
  httpMapping.Add(method, uri, std::move(processorFactory));
}

void Server::Add(HttpMethod method, const std::string& uri, std::function<void(HttpRequest&&, HttpSender&)> f) {
  httpMapping.Add(method, uri,
      std::make_unique<LambdaProcessorFactoryWrapper<HttpProcessorFactory, HttpProcessor, HttpRequest, HttpSender>>(f));
}

void Server::Add(const std::string& uri, std::unique_ptr<WebsocketProcessorFactory> processorFactory) {
  websocketMapping.Add(uri, std::move(processorFactory));
}

void Server::Add(const std::string& uri, std::function<void(WebsocketFrame&&, WebsocketSender&)> f) {
  websocketMapping.Add(uri, std::make_unique<LambdaProcessorFactoryWrapper<WebsocketProcessorFactory,
                                WebsocketProcessor, WebsocketFrame, WebsocketSender>>(f));
}

}  // namespace network
