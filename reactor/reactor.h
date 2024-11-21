#pragma once
#include "../time/time.h"
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <sys/epoll.h>
#include <thread>
#include <vector>

namespace x {
namespace Eventloop {

using Callable = std::function<void()>;
using Stamp = x::time::Stamp;
using Gap = x::time::Gap;
using Fd = int32_t;
using Event = int16_t;
using Iteration = uint64_t;

constexpr uint8_t None = 0;
constexpr uint8_t Read = 1;
constexpr uint8_t Write = 2;
constexpr uint8_t Error = 4;
constexpr uint8_t Timeout = 8;
constexpr uint8_t Close = 16;

class EventView {
public:
  Fd fd;
  Event event;
  Callable callable;
  Iteration iteration;
  EventView(Fd fd, Event event, Callable callable, Iteration iteration)
      : fd(fd), event(event), callable(callable), iteration(iteration) {}
};
class TimeEventView : public EventView {
public:
  Stamp when;
  Gap interval;
  TimeEventView(Fd fd, Event event, Callable callable, Iteration iteration,
                Stamp when, Gap interval)
      : EventView(fd, event, callable, iteration), when(when),
        interval(interval) {}
};

class Reactor {
public:
  Reactor(const Reactor &) = delete;
  Reactor &operator=(const Reactor &) = delete;

  // Owner thread call only
  Reactor(uint16_t m, const Gap &);
  ~Reactor();
  void run();
  void add(Fd, Event, const Callable &);
  void del(Fd, Event);
  void del(Fd); // delete common fd
  Event get(Fd) const;
  EventView get(Fd, Event) const; // user should make sure event exist

  // any thread
  void stop();
  TimeEventView check(Fd);
  Fd plan(const Callable &, const Stamp &, const Gap &);
  void cancel(Fd); // delete time fd

  bool isOwner() const;

protected:
  const int Max_Events;
  const Gap Max_Timeout;
  const int epoll_fd;
  std::atomic<bool> running_;
  std::atomic<Iteration> iteration_;
  std::map<Fd, std::map<Event, std::pair<Iteration, Callable>>>
      fd_event_callable_;
  std::map<Fd, std::pair<Stamp, Gap>> timefd_info_;
  mutable std::mutex mutex_;
};
}; // namespace Eventloop
}; // namespace x
