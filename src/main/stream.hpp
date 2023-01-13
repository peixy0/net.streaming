#pragma once

#include <deque>
#include <set>
#include <thread>
#include "event_queue.hpp"
#include "network.hpp"
#include "recorder.hpp"
#include "video.hpp"

namespace application {

class AppStreamDistributer;

class AppStreamSubscriber : public network::RawStream {
public:
  AppStreamSubscriber(AppStreamDistributer&, network::SenderNotifier&);
  ~AppStreamSubscriber() override;
  std::optional<std::string> GetBuffered() override;
  void ProcessFrame(std::string_view);

private:
  AppStreamDistributer& distributer;
  network::SenderNotifier& notifier;
  std::deque<std::string> streamBuffer;
  std::mutex bufferMut;
};

class AppStreamSubscriberFactory : public network::RawStreamFactory {
public:
  explicit AppStreamSubscriberFactory(AppStreamDistributer&);
  std::unique_ptr<network::RawStream> GetStream(network::SenderNotifier&) override;

private:
  AppStreamDistributer& distributer;
};

class AppStreamDistributer {
public:
  void ProcessBuffer(std::string_view);
  void AddSubscriber(AppStreamSubscriber*);
  void RemoveSubscriber(AppStreamSubscriber*);
  std::string GetSnapshot() const;

private:
  void NotifySubscribers(std::string_view) const;
  void SaveSnapshot(std::string_view);

  std::set<AppStreamSubscriber*> subscribers;
  std::mutex mutable subscribersMut;

  std::string snapshot;
  std::mutex mutable snapshotMut;
};

class AppStreamCapturerRunner : public video::StreamProcessor {
public:
  AppStreamCapturerRunner(const video::StreamOptions&, AppStreamDistributer&, common::EventQueue<RecorderEvent>&);
  void Run();
  void ProcessFrame(std::string_view) override;

private:
  const video::StreamOptions streamOptions;

  std::thread streamThread;
  AppStreamDistributer& streamDistributer;

  bool recorderRunning{false};
  common::EventQueue<RecorderEvent>& recorderEventQueue;
};

}  // namespace application
