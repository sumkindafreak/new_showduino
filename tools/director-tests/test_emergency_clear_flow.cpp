#include <cassert>
#include <cstdint>
#include "../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorEmergencyClearFlow.h"
int main() {
  using F = DirectorEmergencyClearFlow;
  F f;
  assert(!f.confirm(0)); assert(!f.acknowledge(0));
  // Lost request or response: restore retry, ignore a late acknowledgement.
  f.request(1); assert(!f.tick(4000)); assert(f.tick(4001));
  assert(!f.acknowledge(4002)); assert(!f.confirm(4003));
  // Accepted request still needs a distinct operator action.
  f.request(5000); assert(f.acknowledge(5001));
  assert(!f.acknowledge(5002)); assert(f.phase() == F::Phase::Confirm);
  assert(f.confirm(5003)); assert(!f.confirm(5004));
  // Lost confirmation or clear result: timeout, never resend confirmation.
  assert(f.tick(9003)); assert(!f.confirm(9004));
  f.request(10000); assert(f.acknowledge(10001)); assert(f.tick(20001));
  assert(!f.confirm(20002));
  // Reassertion, rejected send, or cancellation invalidates the workflow.
  for (int i=0; i<100; ++i) {
    uint32_t t=30000+i*100;
    f.request(t); assert(f.acknowledge(t+1)); f.cancel();
    assert(!f.confirm(t+2));
    f.request(t+3); assert(f.acknowledge(t+4)); assert(f.confirm(t+5));
    f.cancel(); assert(!f.tick(t+6));
  }
  // millis rollover must not make a permanent wait.
  f.request(UINT32_MAX-100); assert(!f.tick(100)); assert(f.tick(3900));
  f.request(0); assert(!f.acknowledge(4000)); assert(f.tick(4000));
}
