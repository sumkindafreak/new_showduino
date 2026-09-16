#include "EmergencyInput.h"
#include "../../../protocol/showduino_emergency_button.h"
#include "nodes/AudioNodeLink.h"

extern bool emergencyLocked;

/*
 * The P4 emergency latch is authoritative for every specialist node.
 *
 * EMERGENCY:STOP / CLEAR are sent immediately on the real emergency edge by
 * the Stage Engine. This low-rate reconciliation closes the remaining failure
 * mode: a node can reboot, reconnect, or miss the one CLEAR packet and then
 * keep reporting a stale EMERGENCY state forever.
 *
 * Only mismatches transmit. Normal healthy operation adds no radio traffic.
 * Interrupted programme audio is never resumed here; Audio Node CLEAR returns
 * the node to safe IDLE (or leaves FAULT / NO_STORAGE intact).
 */
static uint32_t sAudioEmergencySyncMs = 0;

static void reconcileAudioNodeEmergency(uint32_t nowMs) {
  const AudioNodeStatus &audio = audioNodeLinkStatus();
  if (!audio.online) return;

  const bool nodeEmergency = strcmp(audio.state, "EMERGENCY") == 0;
  if (nodeEmergency == emergencyLocked) return;

  /* Retry at most once per second until the Audio Node reports the P4 truth. */
  if (sAudioEmergencySyncMs != 0 &&
      (nowMs - sAudioEmergencySyncMs) < 1000UL) {
    return;
  }
  sAudioEmergencySyncMs = nowMs;

  Serial.printf("[ESTOP] Audio Node emergency resync P4=%s node=%s -> %s\n",
                emergencyLocked ? "ACTIVE" : "CLEAR",
                audio.state[0] ? audio.state : "UNKNOWN",
                emergencyLocked ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR");
  audioNodeLinkOnEmergency(emergencyLocked);
}

#if SHOWDUINO_ESTOP_GPIO >= 0
#include "driver/gpio.h"

static int sRaw = -1;
static int sStable = -1;
static uint32_t sEdgeMs = 0;
static ShowduinoEstopHoldState sHold;
static ShowduinoEstopClearAuth sClear;
static uint32_t sAssertionSeq = 0;
static bool sClearSuperseded = false;

static bool loopIsOpenLevel(int raw) {
  return raw == SHOWDUINO_ESTOP_ASSERTED_LEVEL;
}

static void cancelPendingClear() {
  showduino_estop_clear_cancel(&sClear);
}

static void noteAssertionLocked() {
  sAssertionSeq++;
  if (sClear.pending) {
    cancelPendingClear();
    sClearSuperseded = true;
  }
}

void emergencyInputBegin() {
  sRaw = -1;
  sStable = -1;
  sEdgeMs = millis();
  showduino_estop_hold_reset(&sHold);
  showduino_estop_clear_reset(&sClear);
  sAssertionSeq = 0;
  sClearSuperseded = false;
  sAudioEmergencySyncMs = 0;

  gpio_reset_pin((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  gpio_set_direction((gpio_num_t)SHOWDUINO_ESTOP_GPIO, GPIO_MODE_INPUT);
  gpio_pullup_en((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  gpio_pulldown_dis((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  pinMode(SHOWDUINO_ESTOP_GPIO, SHOWDUINO_ESTOP_PIN_MODE);

  const int sample = digitalRead(SHOWDUINO_ESTOP_GPIO);
  Serial.println("[ESTOP] Physical pushbutton initialized");
  Serial.printf("[ESTOP] GPIO=%d mode=%s pressed=LOW/emergency released=HIGH/healthy sample=%d\n",
                SHOWDUINO_ESTOP_GPIO,
                SHOWDUINO_ESTOP_PIN_MODE == INPUT_PULLUP ? "INPUT_PULLUP" : "INPUT",
                sample);
  Serial.printf("[ESTOP] Locate hold %lu ms; physical button never clears\n",
                (unsigned long)SHOWDUINO_ESTOP_LOCATE_HOLD_MS);
}

EmergencyInputEvents emergencyInputService(uint32_t nowMs) {
  EmergencyInputEvents ev = {};
  const int raw = digitalRead(SHOWDUINO_ESTOP_GPIO);
  const int open = loopIsOpenLevel(raw) ? 1 : 0;

  if (open != sRaw) {
    sRaw = open;
    sEdgeMs = nowMs;
  }
  if ((nowMs - sEdgeMs) >= SHOWDUINO_ESTOP_DEBOUNCE_MS && sStable != open) {
    const int prev = sStable;
    sStable = open;
    ShowduinoEstopHoldEvents holdEv = {};
    if (open) {
      Serial.println("[ESTOP] Button PRESSED");
      ev.loopOpened = true;
      noteAssertionLocked();
      showduino_estop_hold_on_press(&sHold, nowMs, &holdEv);
      Serial.println("[ESTOP] Locate hold started");
    } else {
      Serial.println("[ESTOP] Button RELEASED");
      if (prev == 1) ev.loopClosed = true;
      showduino_estop_hold_on_release(&sHold, &holdEv);
    }
  }

  ShowduinoEstopHoldEvents holdEv = {};
  showduino_estop_hold_tick(&sHold, nowMs, &holdEv);
  if (holdEv.locateRequested) {
    Serial.printf("[ESTOP] Locate threshold reached %lu ms\n",
                  (unsigned long)SHOWDUINO_ESTOP_LOCATE_HOLD_MS);
    ev.locateRequested = true;
  }

  if (showduino_estop_clear_tick(&sClear, nowMs)) {
    ev.clearExpired = true;
  }
  if (sClearSuperseded) {
    ev.clearSuperseded = true;
    sClearSuperseded = false;
  }

  reconcileAudioNodeEmergency(nowMs);
  return ev;
}

bool emergencyInputLoopOpen() {
  if (sStable >= 0) return sStable == 1;
  return loopIsOpenLevel(digitalRead(SHOWDUINO_ESTOP_GPIO));
}

bool emergencyInputLoopHealthy() {
  return !emergencyInputLoopOpen();
}

int emergencyInputRawGpio() {
  return digitalRead(SHOWDUINO_ESTOP_GPIO);
}

int emergencyInputStableOpen() {
  return sStable;
}

bool emergencyInputLocateHoldActive() {
  return showduino_estop_hold_locate_active(&sHold) != 0;
}

bool emergencyInputLocateHoldFired() {
  return sHold.locateHoldFired != 0;
}

bool emergencyInputPendingClear() {
  return sClear.pending != 0;
}

bool emergencyInputPendingClearValid(uint32_t nowMs) {
  return showduino_estop_clear_pending_valid(&sClear, nowMs) != 0;
}

int emergencyInputBeginDirectorClear(uint32_t nowMs) {
  return showduino_estop_clear_begin(&sClear, nowMs, emergencyLocked ? 1 : 0,
                                     emergencyInputLoopOpen() ? 1 : 0,
                                     sAssertionSeq);
}

int emergencyInputConfirmDirectorClear(uint32_t nowMs) {
  return showduino_estop_clear_confirm(&sClear, nowMs, emergencyLocked ? 1 : 0,
                                       emergencyInputLoopOpen() ? 1 : 0,
                                       sAssertionSeq);
}

void emergencyInputCancelClear() {
  cancelPendingClear();
}

void emergencyInputNoteAssertion() {
  noteAssertionLocked();
}

bool emergencyInputTakeClearSuperseded() {
  const bool v = sClearSuperseded;
  sClearSuperseded = false;
  return v;
}

uint32_t emergencyInputAssertionSeq() {
  return sAssertionSeq;
}

#else

void emergencyInputBegin() {
  sAudioEmergencySyncMs = 0;
}

EmergencyInputEvents emergencyInputService(uint32_t nowMs) {
  EmergencyInputEvents ev = {};
  reconcileAudioNodeEmergency(nowMs);
  return ev;
}

bool emergencyInputLoopOpen() { return false; }
bool emergencyInputLoopHealthy() { return true; }
int emergencyInputRawGpio() { return -1; }
int emergencyInputStableOpen() { return -1; }
bool emergencyInputLocateHoldActive() { return false; }
bool emergencyInputLocateHoldFired() { return false; }
bool emergencyInputPendingClear() { return false; }
bool emergencyInputPendingClearValid(uint32_t) { return false; }
int emergencyInputBeginDirectorClear(uint32_t) {
  return SHOWDUINO_ESTOP_CLEAR_ERR_NOT_LATCHED;
}
int emergencyInputConfirmDirectorClear(uint32_t) {
  return SHOWDUINO_ESTOP_CLEAR_ERR_NO_REQUEST;
}
void emergencyInputCancelClear() {}
void emergencyInputNoteAssertion() {}
bool emergencyInputTakeClearSuperseded() { return false; }
uint32_t emergencyInputAssertionSeq() { return 0; }

#endif
