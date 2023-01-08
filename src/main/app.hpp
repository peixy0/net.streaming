#pragma once
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include "network.hpp"
#include "video.hpp"

namespace application {

class AppStreamProcessor;

class AppStreamSubscriber : public network::RawStream {
public:
  AppStreamSubscriber(AppStreamProcessor&, network::SenderNotifier&);
  ~AppStreamSubscriber() override;
  std::optional<std::string> GetBuffered() override;
  void ProcessFrame(std::string_view);

private:
  AppStreamProcessor& processor;
  network::SenderNotifier& notifier;
  std::deque<std::string> streamBuffer{
      "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=FRAMEBOUNDARY\r\n\r\n"};
  std::mutex bufferMut;
};

class AppStreamSubscriberFactory : public network::RawStreamFactory {
public:
  explicit AppStreamSubscriberFactory(AppStreamProcessor&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&);

private:
  AppStreamProcessor& processor;
};

class AppStreamProcessor : public video::StreamProcessor {
public:
  AppStreamProcessor();
  void AddSubscriber(AppStreamSubscriber*);
  void RemoveSubscriber(AppStreamSubscriber*);
  void StartStream(video::StreamOptions&& options);
  void ProcessFrame(std::string_view) override;
  std::string GetSnapshot() const;

private:
  void NotifySubscribers(std::string_view);
  void SaveSnapshot(std::string_view);

  std::thread streamThread;
  std::atomic<bool> streamRunning;
  std::set<AppStreamSubscriber*> subscribers;
  std::mutex mutable subscribersMut;
  std::string snapshot;
  std::mutex mutable snapshotMut;
};

class AppLayer : public network::HttpProcessor {
public:
  explicit AppLayer(AppStreamProcessor&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  network::HttpResponse BuildPlainTextRequest(network::HttpStatus, std::string_view) const;

  AppStreamProcessor& streamProcessor;
};

}  // namespace application
