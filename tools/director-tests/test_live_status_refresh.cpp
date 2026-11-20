#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorLiveStatusRefresh.h"

int main() {
  using Refresh = DirectorLiveStatusRefresh;
  using State = Refresh::State;
  Refresh refresh;
  assert(refresh.state() == State::Idle);
  refresh.received(10);
  assert(refresh.state() == State::Idle);

  assert(refresh.request(100));
  assert(!refresh.request(200));
  assert(!refresh.tick(3099));
  refresh.received(3099);
  assert(refresh.state() == State::Received);
  assert(!refresh.tick(5598));
  assert(refresh.tick(5599));
  assert(refresh.state() == State::Idle);

  assert(refresh.request(6000));
  assert(refresh.tick(9000));
  assert(refresh.state() == State::TimedOut);
  refresh.received(9001);
  assert(refresh.state() == State::TimedOut);
  assert(refresh.request(9100));
  refresh.sendFailed(9101);
  assert(refresh.state() == State::SendFailed);
  refresh.received(9102);
  assert(refresh.state() == State::SendFailed);
  assert(refresh.tick(11601));
  assert(refresh.state() == State::Idle);

  assert(refresh.request(12000));
  refresh.received(15000);
  assert(refresh.state() == State::TimedOut);
  assert(refresh.request(UINT32_MAX - 100));
  assert(!refresh.tick(2898));
  assert(refresh.tick(2899));
  assert(refresh.state() == State::TimedOut);
  assert(refresh.request(UINT32_MAX - 100));
  refresh.received(100);
  assert(refresh.state() == State::Received);
  assert(refresh.tick(2600));
  assert(refresh.state() == State::Idle);
  std::puts("PASS Live status refresh: reply, rejection, retry, timeout and rollover");
}
