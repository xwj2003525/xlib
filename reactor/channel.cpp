#include "channel.h"
#include "aux/aux.h"
#include "types.h"
#include <memory>
#include <sys/epoll.h>
#include <unistd.h>

x::Eventloop::Channel::~Channel() {
  if (this->close) {
    (*this->close)();
  }
}

void x::Eventloop::Channel::operator()() const {
  if (revents | EPOLLERR) {
    if (error)
      (*error)();
  }
}

void x::Eventloop::FdChannel::operator()() const {
  Channel::operator()();

  if (revents | EPOLLIN) {
    (*read)();
  }

  if (revents | EPOLLOUT) {
    (*write)();
  }
}

void x::Eventloop::TcpChannel::operator()() const {
  FdChannel::operator()();
  if (revents | EPOLLRDHUP) {
    (*rd_hup)();
  }

  if (revents | EPOLLHUP) {
    (*hup)();
  }
}

void x::Eventloop::TimeChannel::operator()() const {
  Channel::operator()();
  if (revents | EPOLLIN) {
    (*times_out)();
    uint64_t i;
    read(fd, &i, sizeof(i));
  }
}

x::Eventloop::TimeChannel plan(const x::time::Stamp &s, const x::time::Gap &g) {
  auto fd = x::Eventloop::Aux::timefd_get(s.MilliSecondsSinceEpoch(),
                                          g.MilliSeconds());
  x::Eventloop::TimeChannel tc;
  tc.fd = fd;
  tc.events |= EPOLLIN;

  x::Eventloop::Callable close_ = [fd]() { close(fd); };
  tc.close = std::make_shared<const x::Eventloop::Callable>(close_);
  return tc;
}
