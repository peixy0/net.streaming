#include "app.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include "network.hpp"
#include "video.hpp"

namespace application {

AppStreamSubscriber::AppStreamSubscriber(AppStreamProcessor& processor, network::SenderNotifier& notifier)
    : processor{processor}, notifier{notifier} {
  processor.AddSubscriber(this);
}

AppStreamSubscriber::~AppStreamSubscriber() {
  processor.RemoveSubscriber(this);
}

std::optional<std::string> AppStreamSubscriber::GetBuffered() {
  bool bufferEmpty{false};
  std::optional<std::string> result;
  {
    std::lock_guard lock{bufferMut};
    if (streamBuffer.empty()) {
      return "";
    }
    result = streamBuffer.front();
    streamBuffer.pop_front();
    bufferEmpty = streamBuffer.empty();
  }
  if (bufferEmpty) {
    notifier.UnmarkPending();
  }
  return result;
}

void AppStreamSubscriber::ProcessFrame(std::string_view frame) {
  std::string buf = "--FRAMEBOUNDARY\r\n";
  buf += "Content-Type: image/jpeg";
  buf += "\r\n\r\n";
  buf += frame;
  buf += "\r\n\r\n";
  {
    std::lock_guard lock{bufferMut};
    streamBuffer.emplace_back(std::move(buf));
  }
  notifier.MarkPending();
}

AppStreamSubscriberFactory::AppStreamSubscriberFactory(AppStreamProcessor& processor) : processor{processor} {
}

std::unique_ptr<network::RawStream> AppStreamSubscriberFactory::GetStream(network::SenderNotifier& notifier) {
  return std::make_unique<AppStreamSubscriber>(processor, notifier);
}

AppStreamProcessor::AppStreamProcessor(video::Stream& stream) : stream{stream} {
  streamThread = std::thread([this] {
    while (true) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(20ms);
      {
        spdlog::debug("processing new frame");
        this->stream.ProcessFrame(*this);
      }
    }
  });
}

void AppStreamProcessor::AddSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.emplace(subscriber);
}

void AppStreamProcessor::RemoveSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.erase(subscriber);
}

void AppStreamProcessor::ProcessFrame(std::string_view frame) {
  std::lock_guard lock{subscribersMut};
  for (auto* s : subscribers) {
    s->ProcessFrame(frame);
  }
}

AppLayer::AppLayer(AppStreamProcessor& streamProcessor) : streamProcessor{streamProcessor} {
}

network::HttpResponse AppLayer::Process(const network::HttpRequest& req) {
  spdlog::debug("app received request {} {} {}", req.method, req.uri, req.version);
  if (req.uri == "/") {
    network::FileHttpResponse resp;
    resp.path = "index.html";
    resp.headers.emplace("Content-Type", "text/html");
    return resp;
  }
  if (req.uri == "/stream") {
    return network::RawStreamHttpResponse{std::make_unique<AppStreamSubscriberFactory>(streamProcessor)};
  }
  network::PreparedHttpResponse resp;
  resp.status = network::HttpStatus::NotFound;
  resp.headers.emplace("Content-Type", "text/plain");
  resp.body = "Not Found";
  return resp;
}

}  // namespace application
