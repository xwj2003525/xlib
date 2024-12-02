#pragma once

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
  virtual void operator()() const = 0;
};

// channel 共享 Callable 于 heap ， 爽
class FdChannel : public Channel {
public:
  virtual ~FdChannel();
  virtual void operator()() const override;
  std::shared_ptr<Callable> read;
  std::shared_ptr<Callable> write;
  std::shared_ptr<Callable> error;
};

class TcpChannel : public FdChannel {
public:
  virtual ~TcpChannel();
  virtual void operator()() const override;
  std::shared_ptr<Callable> hup;
  std::shared_ptr<Callable> rd_hup;
};

}; // namespace Eventloop
}; // namespace x
