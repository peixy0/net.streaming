#include "stream.hpp"
#include <spdlog/spdlog.h>

namespace application {

AppStreamSubscriber::AppStreamSubscriber(AppStreamDistributer& distributer, network::SenderNotifier& notifier)
    : distributer{distributer}, notifier{notifier} {
  distributer.AddSubscriber(this);
  spdlog::info("stream subscriber added");
  streamBuffer.emplace_back(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=FB\r\n\r\n");
}

AppStreamSubscriber::~AppStreamSubscriber() {
  distributer.RemoveSubscriber(this);
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

void AppStreamSubscriber::Notify(std::string_view frame) {
  std::string buf = "--FB\r\nContent-Type: image/jpeg\r\n\r\n";
  buf += frame;
  buf += "\r\n\r\n";
  {
    std::lock_guard lock{bufferMut};
    streamBuffer.emplace_back(std::move(buf));
  }
  notifier.MarkPending();
}

AppStreamSubscriberFactory::AppStreamSubscriberFactory(AppStreamDistributer& distributer) : distributer{distributer} {
}

std::unique_ptr<network::RawStream> AppStreamSubscriberFactory::GetStream(network::SenderNotifier& notifier) {
  return std::make_unique<AppStreamSubscriber>(distributer, notifier);
}

void AppStreamDistributer::Process(std::string_view buffer) {
  static int skip = 0;
  if (skip++ >= 1) {
    skip = 0;
    NotifySubscribers(buffer);
    SaveSnapshot(buffer);
  }
}

void AppStreamDistributer::AddSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.emplace(subscriber);
}

void AppStreamDistributer::RemoveSubscriber(AppStreamSubscriber* subscriber) {
  std::lock_guard lock{subscribersMut};
  subscribers.erase(subscriber);
}

void AppStreamDistributer::SaveSnapshot(std::string_view frame) {
  std::lock_guard lock{snapshotMut};
  snapshot = frame;
}

std::string AppStreamDistributer::GetSnapshot() const {
  std::lock_guard lock{snapshotMut};
  std::string copy{snapshot};
  return copy;
}

void AppStreamDistributer::NotifySubscribers(std::string_view frame) const {
  std::lock_guard lock{subscribersMut};
  for (auto* s : subscribers) {
    s->Notify(frame);
  }
}

AppStreamCapturerRunner::AppStreamCapturerRunner(const video::StreamOptions& streamOptions,
                                                 AppStreamDistributer& streamDistributer,
                                                 common::EventQueue<RecordingEvent>& recorderEventQueue)
    : streamOptions{streamOptions}, streamDistributer{streamDistributer}, recorderEventQueue{recorderEventQueue} {
}

void AppStreamCapturerRunner::Run() {
  streamThread = std::thread([this, streamOptions = std::move(streamOptions)]() mutable {
    auto device = video::Device("/dev/video0");
    auto stream = device.GetStream(std::move(streamOptions));
    while (true) {
      stream.ProcessFrame(*this);
    }
  });
}

void AppStreamCapturerRunner::ProcessFrame(std::string_view frame) {
  streamDistributer.Process(frame);
  recorderEventQueue.Push(RecordingAppend{{frame.data(), frame.size()}});
}

}  // namespace application
