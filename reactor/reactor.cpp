#include "reactor.h"
#include <assert.h>
#include <bits/types/struct_itimerspec.h>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

static thread_local x::Eventloop::Reactor *reactor_in_this_thread = nullptr;

inline static bool isReactorMatchThread(const x::Eventloop::Reactor *r) {
  return r == reactor_in_this_thread;
}

static void add_fd_to_epoll(int epoll_fd, int fd, int events) {
  struct epoll_event ee;
  ee.events = events;
  ee.data.fd = fd;

  auto ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ee);
  assert(ret != -1);
}

static void del_fd_to_epoll(int epoll_fd, int fd) {
  auto ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
  assert(ret != -1);
}

static int create_timefd(const x::time::Stamp &s, const x::time::Gap &g) {
  int time_fd = timerfd_create(CLOCK_REALTIME, 0);
  assert(time_fd != -1);

  struct itimerspec tm;
  memset(&tm, 0, sizeof(tm));

  auto milliseconds_since_epoch = s.MilliSecondsSinceEpoch();
  tm.it_value.tv_sec = milliseconds_since_epoch / 1000;
  tm.it_value.tv_nsec = (milliseconds_since_epoch % 1000) * 1000000;

  auto milliseconds = g.MilliSeconds();
  tm.it_interval.tv_sec = milliseconds / 1000;
  tm.it_interval.tv_nsec = (milliseconds % 1000) * 1000000;

  auto ret = timerfd_settime(time_fd, TIMER_ABSTIME, &tm, NULL);

  assert(ret != -1);
  return time_fd;
}

x::Eventloop::Reactor::Reactor(uint16_t m, const Gap &g)
    : Max_Events(m), Max_Timeout(g), epoll_fd(epoll_create1(0)),
      running_(false), iteration_(0) {

  assert(epoll_fd != -1);
  assert(reactor_in_this_thread == nullptr);
  reactor_in_this_thread = this;
}

bool x::Eventloop::Reactor::isOwner() const {
  return isReactorMatchThread(this);
}

x::Eventloop::Reactor::~Reactor() {
  assert(isReactorMatchThread(this));

  {
    std::lock_guard<std::mutex> a(mutex_);
    for (const auto &[k, v] : timefd_info_) {
      close(k);
    }
  }

  close(epoll_fd);
  reactor_in_this_thread = nullptr;
}

void x::Eventloop::Reactor::add(Fd f, Event e, const Callable &c) {
  assert(isReactorMatchThread(this));
  add_fd_to_epoll(epoll_fd, f, e);

  {
    std::lock_guard<std::mutex> a(mutex_);
    fd_event_callable_[f][e] = {0, c};
  }
}

void x::Eventloop::Reactor::del(Fd f) {
  assert(isReactorMatchThread(this));
  del_fd_to_epoll(epoll_fd, f);

  {
    std::lock_guard<std::mutex> a(mutex_);
    fd_event_callable_.erase(f);
  }
}

void x::Eventloop::Reactor::del(Fd f, Event e) {
  assert(isReactorMatchThread(this));

  {
    std::lock_guard<std::mutex> a(mutex_);
    auto i = fd_event_callable_.find(f);
    if (i != fd_event_callable_.end()) {
      (*i).second.erase(e);
      if (i->second.empty()) {
        fd_event_callable_.erase(i);
      }
    }
  }
}

x::Eventloop::Event x::Eventloop::Reactor::get(Fd f) const {
  assert(isReactorMatchThread(this));

  Event ret = 0;

  {
    std::lock_guard<std::mutex> a(mutex_);
    auto i = fd_event_callable_.find(f);
    if (i != fd_event_callable_.end()) {
      for (const auto &j : i->second) {
        ret |= j.first;
      }
    }
  }

  return ret;
}

x::Eventloop::EventView x::Eventloop::Reactor::get(Fd f, Event e) const {
  assert(isReactorMatchThread(this));

  std::pair<Iteration, Callable> pair;
  {
    std::lock_guard<std::mutex> a(mutex_);
    pair = fd_event_callable_.at(f).at(e);
  }
  return {f, e, pair.second, pair.first};
}

void x::Eventloop::Reactor::run() {
  assert(!running_);
  assert(isOwner());

  running_ = true;
  std::vector<struct epoll_event> events(Max_Events);

  while (running_) {
    int nfd = epoll_wait(epoll_fd, events.data(), Max_Events,
                         Max_Timeout.MilliSeconds());

    assert(nfd != -1);
    for (int i = 0; i < nfd; i++) {
      auto fd = events[i].data.fd;
      if (events[i].events & (Read | Write | Error | Timeout | Close)) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &fd_map = fd_event_callable_[fd];

        if (events[i].events & Read) {
          if (fd_map.count(Read)) {
            auto &[iteration, callable] = fd_map[Read];
            iteration++;
            callable();
          }

          // timefd超时后要read一下
          if (timefd_info_.count(fd)) {
            uint64_t expirations;
            ssize_t s = read(fd, &expirations, sizeof(expirations));
          }
        }
        if (events[i].events & Write) {
          if (fd_map.count(Write)) {
            auto &[iteration, callable] = fd_map[Write];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Error) {
          if (fd_map.count(Error)) {
            auto &[iteration, callable] = fd_map[Error];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Timeout) {
          if (fd_map.count(Timeout)) {
            auto &[iteration, callable] = fd_map[Timeout];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Close) {
          if (fd_map.count(Close)) {
            auto &[iteration, callable] = fd_map[Close];
            iteration++;
            callable();
          }
        }
      }
    }
    ++iteration_;
  }
}

x::Eventloop::Fd x::Eventloop::Reactor::plan(const Callable &c, const Stamp &s,
                                             const Gap &g) {
  auto time_fd = create_timefd(s, g);
  add_fd_to_epoll(epoll_fd, time_fd, Read);

  {
    std::lock_guard<std::mutex> a(mutex_);
    fd_event_callable_[time_fd][Read] = {0, c};
    timefd_info_[time_fd] = {s, g};
  }
  return time_fd;
}

void x::Eventloop::Reactor::cancel(Fd f) {
  std::lock_guard<std::mutex> a(mutex_);
  fd_event_callable_.erase(f);
  timefd_info_.erase(f);
}

void x::Eventloop::Reactor::stop() { running_ = false; }

x::Eventloop::TimeEventView x::Eventloop::Reactor::check(Fd f) {
  std::lock_guard<std::mutex> a(mutex_);
  auto i = fd_event_callable_[f][x::Eventloop::Read];
  auto j = timefd_info_[f];
  return {f, x::Eventloop::Read, i.second, i.first, j.first, j.second};
}
