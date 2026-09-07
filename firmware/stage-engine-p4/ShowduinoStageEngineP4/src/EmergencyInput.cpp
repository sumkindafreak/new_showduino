#include "EmergencyInput.h"

#if SHOWDUINO_ESTOP_GPIO >= 0
#include "driver/gpio.h"

static int sRaw = -1;
static int sStable = -1;
static uint32_t sEdgeMs = 0;
static uint32_t sPressStartMs = 0;
static uint8_t sPressCount = 0;
static uint32_t sSequenceStartMs = 0;
static bool sLongHoldFired = false;
static bool sPendingClear = false;
static uint32_t sPendingClearUntilMs = 0;
static bool sLocateWindowArmed = false;

static bool loopIsOpenLevel(int raw) {
  return raw == SHOWDUINO_ESTOP_ASSERTED_LEVEL;
}

static void resetLocateSequence() {
  sPressCount = 0;
  sSequenceStartMs = 0;
  sLocateWindowArmed = false;
}

static void cancelPendingClear() {
  sPendingClear = false;
  sPendingClearUntilMs = 0;
}

static void beginPendingClear(uint32_t nowMs) {
  sPendingClear = true;
  sPendingClearUntilMs = nowMs + SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS;
}

static void notePress(uint32_t nowMs, EmergencyInputEvents *ev) {
  if (!sLocateWindowArmed) {
    sLocateWindowArmed = true;
    sSequenceStartMs = nowMs;
    sPressCount = 1;
  } else if ((nowMs - sSequenceStartMs) >= SHOWDUINO_ESTOP_LOCATE_WINDOW_MS) {
    sSequenceStartMs = nowMs;
    sPressCount = 1;
  } else {
    if (sPressCount < 255) sPressCount++;
  }

  Serial.printf("[ESTOP] Locate sequence %u/%u\n",
                (unsigned)sPressCount,
                (unsigned)SHOWDUINO_ESTOP_LOCATE_PRESS_COUNT);

  if (sPressCount >= SHOWDUINO_ESTOP_LOCATE_PRESS_COUNT) {
    ev->locateRequested = true;
    resetLocateSequence();
  }
}

void emergencyInputBegin() {
  sRaw = -1;
  sStable = -1;
  sEdgeMs = millis();
  sPressStartMs = 0;
  sLongHoldFired = false;
  resetLocateSequence();
  cancelPendingClear();

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
    if (open) {
      Serial.println("[ESTOP] Button PRESSED");
      ev.loopOpened = true;
      sPressStartMs = nowMs;
      sLongHoldFired = false;
      notePress(nowMs, &ev);
    } else {
      Serial.println("[ESTOP] Button RELEASED");
      if (prev == 1) ev.loopClosed = true;
      sLongHoldFired = false;
      sPressStartMs = 0;
    }
  }

  if (sLocateWindowArmed &&
      (nowMs - sSequenceStartMs) >= SHOWDUINO_ESTOP_LOCATE_WINDOW_MS) {
    resetLocateSequence();
  }

  if (sStable == 1 && !sLongHoldFired && sPressStartMs != 0 &&
      (nowMs - sPressStartMs) >= SHOWDUINO_ESTOP_CLEAR_HOLD_MS) {
    sLongHoldFired = true;
    resetLocateSequence();
    if (!sPendingClear) {
      beginPendingClear(nowMs);
      ev.clearRequested = true;
    }
  }

  if (sPendingClear && (int32_t)(nowMs - sPendingClearUntilMs) >= 0) {
    cancelPendingClear();
    ev.clearExpired = true;
  }

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

uint8_t emergencyInputLocateCount() {
  return sPressCount;
}

bool emergencyInputPendingClear() {
  return sPendingClear;
}

bool emergencyInputPendingClearValid(uint32_t nowMs) {
  if (!sPendingClear) return false;
  return (int32_t)(nowMs - sPendingClearUntilMs) < 0;
}

void emergencyInputCancelClear() {
  cancelPendingClear();
}

#else

void emergencyInputBegin() {}

EmergencyInputEvents emergencyInputService(uint32_t) {
  EmergencyInputEvents ev = {};
  return ev;
}

bool emergencyInputLoopOpen() { return false; }
bool emergencyInputLoopHealthy() { return true; }
int emergencyInputRawGpio() { return -1; }
int emergencyInputStableOpen() { return -1; }
uint8_t emergencyInputLocateCount() { return 0; }
bool emergencyInputPendingClear() { return false; }
bool emergencyInputPendingClearValid(uint32_t) { return false; }
void emergencyInputCancelClear() {}

#endif
