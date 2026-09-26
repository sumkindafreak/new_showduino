#include "EmergencyInput.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_emergency_node.h"

static int sPressed = 0;
static int sRaw = 0;
static int sDebounced = 0;
static uint32_t sLastPoll = 0;
static uint32_t sEdgeMs = 0;
static uint32_t sRearmDownMs = 0;

void emergencyInputBegin() {
  pinMode(SHOWDUINO_ESTOP_NODE_GPIO, SHOWDUINO_ESTOP_NODE_PIN_MODE);
#if SHOWDUINO_ESTOP_NODE_REARM_GPIO >= 0
  pinMode(SHOWDUINO_ESTOP_NODE_REARM_GPIO, INPUT_PULLUP);
#endif
  sRaw = digitalRead(SHOWDUINO_ESTOP_NODE_GPIO);
  sPressed = (sRaw == SHOWDUINO_ESTOP_NODE_ACTIVE_LEVEL) ? 1 : 0;
  sDebounced = sPressed;
  sLastPoll = 0;
  sEdgeMs = 0;
}

void emergencyInputService() {
  const uint32_t now = millis();
  if (sLastPoll && (now - sLastPoll) < SHOWDUINO_ESTOP_NODE_INPUT_POLL_MS) return;
  sLastPoll = now;
  sRaw = digitalRead(SHOWDUINO_ESTOP_NODE_GPIO);
  const int pressed = (sRaw == SHOWDUINO_ESTOP_NODE_ACTIVE_LEVEL) ? 1 : 0;
  if (pressed != sDebounced) {
    if (!sEdgeMs) sEdgeMs = now;
    if ((now - sEdgeMs) >= SHOWDUINO_EMERGENCY_DEBOUNCE_MS) {
      sDebounced = pressed;
      sPressed = pressed;
      sEdgeMs = 0;
    }
  } else {
    sEdgeMs = 0;
    sPressed = sDebounced;
  }
}

int emergencyInputPressed() { return sPressed; }
int emergencyInputRaw() { return sRaw; }

int emergencyInputRearmHeld(uint32_t nowMs) {
#if SHOWDUINO_ESTOP_NODE_REARM_GPIO < 0
  (void)nowMs;
  return 0;
#else
  /* Maintenance-only. Never clears P4. */
  const int down = (digitalRead(SHOWDUINO_ESTOP_NODE_REARM_GPIO) ==
                    SHOWDUINO_ESTOP_NODE_REARM_ACTIVE);
  if (!down) {
    sRearmDownMs = 0;
    return 0;
  }
  if (!sRearmDownMs) sRearmDownMs = nowMs;
  return ((nowMs - sRearmDownMs) >= SHOWDUINO_ESTOP_NODE_REARM_LONG_MS) ? 1 : 0;
#endif
}
