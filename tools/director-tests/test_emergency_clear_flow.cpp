#include <cassert>
#include <cstdint>
#include "../../protocol/showduino_emergency_button.h"
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
  // Run both production policies together, dropping each protocol leg.
  for (int lost=0; lost<4; ++lost) {
    F director; ShowduinoEstopClearAuth stage{};
    bool latched=true;
    director.request(100);
    if (lost != 0) {
      assert(showduino_estop_clear_begin(&stage,101,latched,0,9)==SHOWDUINO_ESTOP_CLEAR_OK);
      if (lost != 1) {
        assert(director.acknowledge(102)); assert(director.confirm(103));
        if (lost != 2) {
          assert(showduino_estop_clear_confirm(&stage,104,latched,0,9)==SHOWDUINO_ESTOP_CLEAR_OK);
          latched=false; // Only the P4 policy's successful confirm releases it.
          // lost == 3: drop the authorized result back to Director.
        }
      }
    }
    assert(director.tick(5000)); assert(!director.confirm(5001));
    assert(latched == (lost != 3));
    showduino_estop_clear_cancel(&stage);
  }
  // Fresh emergency assertion and held physical input defeat an old confirm.
  ShowduinoEstopClearAuth stage{};
  assert(showduino_estop_clear_begin(&stage,0,1,0,1)==SHOWDUINO_ESTOP_CLEAR_OK);
  assert(showduino_estop_clear_confirm(&stage,1,1,0,2)==SHOWDUINO_ESTOP_CLEAR_ERR_SUPERSEDED);
  assert(showduino_estop_clear_begin(&stage,2,1,0,2)==SHOWDUINO_ESTOP_CLEAR_OK);
  assert(showduino_estop_clear_confirm(&stage,3,1,1,2)==SHOWDUINO_ESTOP_CLEAR_ERR_BUTTON_ACTIVE);
  // millis rollover must not make a permanent wait.
  f.request(UINT32_MAX-100); assert(!f.tick(100)); assert(f.tick(3900));
  f.request(0); assert(!f.acknowledge(4000)); assert(f.tick(4000));
}
