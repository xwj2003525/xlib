#pragma once

#include "../time/time.h"
#include "../tools/copyable.h"
#include "types.h"
#include <memory>

namespace x {
namespace Eventloop {

class Channel : public tools::copyable {
public:
  Fd fd;
  Events events;
  Events revents;

  std::shared_ptr<const Callable> error;
  std::shared_ptr<const Callable> close;

  //多态时，自动调用子类的默认析构，即shared_ptr自动销毁计数-1
  virtual ~Channel();
  virtual void operator()() const;
};

// channel 共享 Callable 于 heap ， 爽

class FdChannel : public Channel {
public:
  virtual void operator()() const override;
  std::shared_ptr<const Callable> read;
  std::shared_ptr<const Callable> write;
};

class TcpChannel : public FdChannel {
public:
  virtual void operator()() const override;
  std::shared_ptr<const Callable> hup;
  std::shared_ptr<const Callable> rd_hup;
};

class TimeChannel : public Channel {
public:
  virtual void operator()() const override;
  std::shared_ptr<const Callable> times_out;
};

TimeChannel plan(const time::Stamp &,
                 const time::Gap & = time::Gap::PlaceHolder());

}; // namespace Eventloop
}; // namespace x
