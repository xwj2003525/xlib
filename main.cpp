#include "log/log.h"
#include "reactor/reactor.h"
#include "time/time.h"
#include <thread>

x::Eventloop::Reactor r(20, x::time::Gap::Seconds(3));

void f() {}

void f1() { r.stop(); }

void a() {
  r.plan(f, x::time::Stamp::Now(), x::time::Gap::Seconds(3));
  r.plan(f1, x::time::Stamp::Now() + x::time::Gap::Seconds(10));
}

void b() { r.stop(); }

int main() {
  log_init();
  std::thread m(a);

  r.run();

  m.join();
  log_finish();
}
