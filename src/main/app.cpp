#include "app.hpp"
#include <spdlog/spdlog.h>
#include "network.hpp"
#include "video.hpp"

namespace application {

AppStreamSubscriber::AppStreamSubscriber(AppStreamProcessor& processor, network::SenderNotifier& notifier)
    : processor{processor}, notifier{notifier} {
  processor.AddSubscriber(this);
  spdlog::info("stream subscriber added");
}

AppStreamSubscriber::~AppStreamSubscriber() {
  processor.RemoveSubscriber(this);
  spdlog::info("stream subscriber removed");
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

AppStreamProcessor::AppStreamProcessor() {
  video::StreamOptions defaultOptions;
  defaultOptions.width = 1280;
  defaultOptions.height = 720;
  defaultOptions.framerate = 30;
  StartStream(std::move(defaultOptions));
}

void AppStreamProcessor::AddSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.emplace(subscriber);
}

void AppStreamProcessor::RemoveSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.erase(subscriber);
}

void AppStreamProcessor::StartStream(video::StreamOptions&& options) {
  streamRunning = false;
  if (streamThread.joinable()) {
    streamThread.join();
  }
  streamRunning = true;
  streamThread = std::thread([this, options = std::move(options)]() mutable {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(std::move(options));
    while (streamRunning) {
      spdlog::debug("processing new frame");
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamProcessor::ProcessFrame(std::string_view frame) {
  NotifySubscribers(frame);
  SaveSnapshot(frame);
}

std::string AppStreamProcessor::GetSnapshot() const {
  std::lock_guard lock{snapshotMut};
  std::string copy{snapshot};
  return copy;
}

void AppStreamProcessor::NotifySubscribers(std::string_view frame) const {
  std::lock_guard lock{subscribersMut};
  for (auto* s : subscribers) {
    s->ProcessFrame(frame);
  }
}

void AppStreamProcessor::SaveSnapshot(std::string_view frame) {
  std::lock_guard lock{snapshotMut};
  snapshot = frame;
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
  if (req.uri == "/snapshot") {
    const auto payload = streamProcessor.GetSnapshot();
    network::PreparedHttpResponse resp;
    resp.status = network::HttpStatus::OK;
    resp.headers.emplace("Content-Type", "image/jpeg");
    resp.body = std::move(payload);
    return resp;
  }
  if (req.uri == "/param") {
    video::StreamOptions options;
    const auto qualityQuery = req.query.find("quality");
    if (qualityQuery == req.query.cend()) {
      return BuildPlainTextRequest(network::HttpStatus::BadRequest, "Bad Request");
    }
    if (qualityQuery->second == "hires") {
      options.width = 1280;
      options.height = 720;
      options.framerate = 30;
    } else if (qualityQuery->second == "lowres") {
      options.width = 848;
      options.height = 480;
      options.framerate = 15;
    } else {
      return BuildPlainTextRequest(network::HttpStatus::BadRequest, "Bad Request");
    }
    streamProcessor.StartStream(std::move(options));
    return BuildPlainTextRequest(network::HttpStatus::OK, "OK");
  }
  return BuildPlainTextRequest(network::HttpStatus::NotFound, "Not Found");
}

network::HttpResponse AppLayer::BuildPlainTextRequest(network::HttpStatus status, std::string_view body) const {
  network::PreparedHttpResponse resp;
  resp.status = status;
  resp.headers.emplace("Content-Type", "text/plain; charset=UTF-8");
  resp.body = body;
  return resp;
}

}  // namespace application
