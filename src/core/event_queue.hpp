#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace common {

template <typename Event>
class EventQueue {
public:
  EventQueue() = default;
  EventQueue(const EventQueue&) = delete;
  virtual ~EventQueue() = default;
  virtual void Push(Event&&) = 0;
  virtual Event Pop() = 0;
  virtual int Size() const = 0;
};

template <typename Event>
class ConcreteEventQueue final : public EventQueue<Event> {
public:
  ConcreteEventQueue() = default;

  void Push(Event&& event) override {
    std::unique_lock lock{mut};
    events.emplace_back(std::move(event));
    lock.unlock();
    cv.notify_one();
  }

  Event Pop() override {
    std::unique_lock lock{mut};
    cv.wait(lock, [this] { return not events.empty(); });
    auto ev = std::move(events.front());
    events.pop_front();
    return ev;
  }

  int Size() const override {
    std::lock_guard lock{mut};
    return events.size();
  }

private:
  std::deque<Event> events;
  mutable std::mutex mut;
  std::condition_variable cv;
};

}  // namespace common
