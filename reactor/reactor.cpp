#include "reactor.h"
#include "../log/log.h"
#include <bits/types/struct_itimerspec.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

static thread_local x::Eventloop::Reactor *reactor_in_this_thread = nullptr;

inline static bool isReactorMatchThread(const x::Eventloop::Reactor *r) {
  return r == reactor_in_this_thread;
}

static void add_fd_to_epoll(int epoll_fd, int fd, int events) {
  struct epoll_event ee;
  ee.events = events;
  ee.data.fd = fd;
  auto ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ee);
  if (ret == -1) {
    LOG(FATAL) << "Failed to add fd to epoll: " << strerror(errno);
  }
}
static void del_fd_to_epoll(int epoll_fd, int fd) {
  auto ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
  if (ret == -1) {
    LOG(FATAL) << "Failed to delete fd from epoll: " << strerror(errno);
  }
}

static int create_timefd(const x::time::Stamp &s, const x::time::Gap &g) {
  int time_fd = timerfd_create(CLOCK_REALTIME, 0);
  if (time_fd == -1) {
    LOG(FATAL) << "Failed to create timefd: " << strerror(errno);
  }
  struct itimerspec tm;
  memset(&tm, 0, sizeof(tm));

  auto milliseconds_since_epoch = s.MilliSecondsSinceEpoch();
  tm.it_value.tv_sec = milliseconds_since_epoch / 1000;
  tm.it_value.tv_nsec = (milliseconds_since_epoch % 1000) * 1000000;

  auto milliseconds = g.MilliSeconds();
  tm.it_interval.tv_sec = milliseconds / 1000;
  tm.it_interval.tv_nsec = (milliseconds % 1000) * 1000000;

  auto ret = timerfd_settime(time_fd, TIMER_ABSTIME, &tm, NULL);
  if (ret == -1) {
    LOG(FATAL) << "Failed to set timerfd time: " << strerror(errno);
  }
  return time_fd;
}

x::Eventloop::Reactor::Reactor(uint16_t m, const Gap &g)
    : Max_Events(m), Max_Timeout(g), epoll_fd(epoll_create1(0)),
      running_(false) {

  if (epoll_fd == -1) {
    LOG(FATAL) << "Failed to create epoll file descriptor: " << strerror(errno);
  }
  if (reactor_in_this_thread != nullptr) {
    LOG(FATAL) << "Another reactor is already running in this thread!";
  }
  reactor_in_this_thread = this;
  LOG(INFO) << "Reactor created with max events: " << Max_Events
            << " max timeout: " << Max_Timeout.MilliSeconds() << "ms";
}

x::Eventloop::Reactor::~Reactor() {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  {
    std::lock_guard<std::mutex> a(mutex_);
    for (const auto &[k, v] : timefd_info_) {
      close(k);
    }
    close(epoll_fd);
    reactor_in_this_thread = nullptr;
  }

  LOG(INFO) << "Reactor destroyed";
}

void x::Eventloop::Reactor::add(Fd f, Event e, const Callable &c) {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  LOG(INFO) << "Adding fd: " << f << " with event: " << e;

  if (!(e & Read || e & Write || e & Error || e & Timeout || e & Close)) {
    LOG(FATAL) << "not valid event input";
  }

  add_fd_to_epoll(epoll_fd, f, e);

  {
    std::lock_guard<std::mutex> a(mutex_);
    fd_event_callable_[f][e] = {0, c};
  }
}

void x::Eventloop::Reactor::del(Fd f) {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  LOG(INFO) << "Deleting fd: " << f;
  del_fd_to_epoll(epoll_fd, f);

  {
    std::lock_guard<std::mutex> a(mutex_);
    fd_event_callable_.erase(f);
    timefd_info_.erase(f);
  }
}

void x::Eventloop::Reactor::del(Fd f, Event e) {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  LOG(INFO) << "Deleting event: " << e << " from fd: " << f;

  del_fd_to_epoll(epoll_fd, f);

  std::lock_guard<std::mutex> a(mutex_);

  auto i = fd_event_callable_.find(f);
  if (i != fd_event_callable_.end()) {
    (*i).second.erase(e);

    //失去最后一个event的监视，自动释放对fd的监视
    if (i->second.empty()) {
      fd_event_callable_.erase(i);
    }
  }
  timefd_info_.erase(f);
}

x::Eventloop::Event x::Eventloop::Reactor::get(Fd f) const {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
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
  LOG(INFO) << "Getting event: " << ret << " from fd: " << f;
  return ret;
}

x::Eventloop::EventView x::Eventloop::Reactor::get(Fd f, Event e) const {
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  LOG(INFO) << "Getting event view for event: " << e << " from fd: " << f;

  std::pair<Iteration, Callable> pair;
  {
    std::lock_guard<std::mutex> a(mutex_);
    pair = fd_event_callable_.at(f).at(e);
  }
  return {f, e, pair.second, pair.first};
}

void x::Eventloop::Reactor::run() {
  if (running_) {
    LOG(FATAL) << "Reactor is already running!";
  }
  if (!isReactorMatchThread(this)) {
    LOG(FATAL) << "Reactor is not matching the thread!";
  }
  LOG(INFO) << "Reactor is running";

  running_ = true;
  std::vector<struct epoll_event> events(Max_Events);

  while (running_) {
    int nfd = epoll_wait(epoll_fd, events.data(), Max_Events,
                         Max_Timeout.MilliSeconds());

    if (nfd == -1) {
      LOG(FATAL) << "epoll_wait failed: " << strerror(errno);
    }

    for (int i = 0; i < nfd; i++) {
      auto fd = events[i].data.fd;
      if (events[i].events & (Read | Write | Error | Timeout | Close)) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &fd_map = fd_event_callable_[fd];

        if (events[i].events & Read) {
          if (fd_map.count(Read)) {
            LOG(INFO) << "fd:" << fd << " trrigger read event";
            auto &[iteration, callable] = fd_map[Read];
            iteration++;
            callable();
          }

          // timefd超时后要read一下
          if (timefd_info_.count(fd)) {
            uint64_t expirations;
            read(fd, &expirations, sizeof(expirations));
          }
        }
        if (events[i].events & Write) {
          if (fd_map.count(Write)) {
            LOG(INFO) << "fd:" << fd << " trrigger write event";
            auto &[iteration, callable] = fd_map[Write];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Error) {
          if (fd_map.count(Error)) {
            LOG(INFO) << "fd:" << fd << " trrigger error event";
            auto &[iteration, callable] = fd_map[Error];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Timeout) {
          if (fd_map.count(Timeout)) {
            LOG(INFO) << "fd:" << fd << " trrigger timeout event";
            auto &[iteration, callable] = fd_map[Timeout];
            iteration++;
            callable();
          }
        }
        if (events[i].events & Close) {
          if (fd_map.count(Close)) {
            LOG(INFO) << "fd:" << fd << " trrigger close event";
            auto &[iteration, callable] = fd_map[Close];
            iteration++;
            callable();
          }
        }
      }
    }
  }

  LOG(INFO) << "Reactor is not running";
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
  LOG(INFO) << "Planned new timer event: " << time_fd;
  return time_fd;
}

void x::Eventloop::Reactor::cancel(Fd f) {
  std::lock_guard<std::mutex> a(mutex_);

  del_fd_to_epoll(epoll_fd, f);
  fd_event_callable_.erase(f);
  timefd_info_.erase(f);
  LOG(INFO) << "Cancelled timer event: " << f;
}

void x::Eventloop::Reactor::stop() {
  LOG(INFO) << "Stopping the reactor.";
  running_ = false;
}

x::Eventloop::TimeEventView x::Eventloop::Reactor::check(Fd f) {
  std::lock_guard<std::mutex> a(mutex_);
  auto i = fd_event_callable_[f][x::Eventloop::Read];
  auto j = timefd_info_[f];
  LOG(INFO) << "Checking timeevent view for fd: " << f;
  return {f, x::Eventloop::Read, i.second, i.first, j.first, j.second};
}
