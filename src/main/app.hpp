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
  AppStreamSubscriberFactory(AppStreamProcessor&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&);

private:
  AppStreamProcessor& processor;
};

class AppStreamProcessor : public video::StreamProcessor {
public:
  AppStreamProcessor(video::Stream&);
  void AddSubscriber(AppStreamSubscriber*);
  void RemoveSubscriber(AppStreamSubscriber*);
  void ProcessFrame(std::string_view) override;

private:
  video::Stream& stream;
  std::thread streamThread;
  std::set<AppStreamSubscriber*> subscribers;
  std::mutex subscribersMut;
};

class AppLayer : public network::HttpProcessor {
public:
  AppLayer(AppStreamProcessor&);
  ~AppLayer() = default;
  network::HttpResponse Process(const network::HttpRequest&) override;

private:
  AppStreamProcessor& streamProcessor;
};

}  // namespace application
