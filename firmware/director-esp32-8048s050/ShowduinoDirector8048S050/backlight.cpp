#include "backlight.h"
#include "DirectorUnlockScreen.h"

/* These are owned by the main Director sketch.  The unlock screen only reads
 * them; it never changes transport, runtime or emergency state. */
extern bool espNowReady;
extern uint8_t linkState;
extern bool emergencyLocked;

enum BlState : uint8_t {
  BL_STATE_FULL = 0,
  BL_STATE_DIM,
  BL_STATE_OFF
};

static uint8_t  s_pin = 255;
static uint16_t s_pwmFull = BL_PWM_MAX;
static uint16_t s_pwmDim = BL_PWM_DIM_FLOOR;
static uint16_t s_pwm = BL_PWM_MAX;
static BlState  s_state = BL_STATE_FULL;
static uint32_t s_lastActive = 0;
static uint32_t s_wakeUntil = 0;
static uint8_t  s_timeoutMin = 10;
static uint8_t  s_brightness = 255;
static bool     s_autoEnabled = true;
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
static uint8_t  s_ledcCh = 0;
#endif

static uint16_t brightnessToPwm(uint8_t b) {
  if (b >= 255) return BL_PWM_MAX;
  return (uint16_t)(((uint32_t)b * BL_PWM_MAX) / 255U);
}

static void applyPwm(uint16_t duty) {
  if (s_pin == 255) return;
  if (duty > BL_PWM_MAX) duty = BL_PWM_MAX;
  s_pwm = duty;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(s_pin, duty);
#else
  ledcWrite(s_ledcCh, duty);
#endif
}

static void setState(BlState st) {
  if (s_state == st) return;
  s_state = st;
  switch (st) {
    case BL_STATE_FULL: applyPwm(s_pwmFull); break;
    case BL_STATE_DIM:  applyPwm(s_pwmDim);  break;
    case BL_STATE_OFF:  applyPwm(0);         break;
  }
}

void backlightInit(uint8_t pin) {
  s_pin = pin;
  pinMode(s_pin, OUTPUT);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(s_pin, 1000, 10);
  ledcWrite(s_pin, BL_PWM_MAX);
#else
  s_ledcCh = 0;
  ledcSetup(s_ledcCh, 1000, 10);
  ledcAttachPin(s_pin, s_ledcCh);
  ledcWrite(s_ledcCh, BL_PWM_MAX);
#endif
  s_state = BL_STATE_FULL;
  s_pwm = BL_PWM_MAX;
  uint32_t now = millis();
  s_lastActive = now;
  s_wakeUntil = now + BL_WAKE_LATCH_MS;
  Serial.printf("Backlight: ON (auto idle pin=%u)\n", (unsigned)s_pin);
}

void backlightConfigure(uint8_t timeoutMinutes, uint8_t brightness255) {
  s_timeoutMin = timeoutMinutes;
  s_brightness = brightness255 ? brightness255 : 1;
  s_autoEnabled = (timeoutMinutes > 0);
  s_pwmFull = brightnessToPwm(s_brightness);
  s_pwmDim = (uint16_t)(s_pwmFull / 6U);
  if (s_pwmDim < BL_PWM_DIM_FLOOR && s_pwmFull > BL_PWM_DIM_FLOOR) {
    s_pwmDim = BL_PWM_DIM_FLOOR;
  }
  if (s_pwmDim >= s_pwmFull) {
    s_pwmDim = (s_pwmFull > 1) ? (s_pwmFull / 2) : 1;
  }

  if (!s_autoEnabled) {
    setState(BL_STATE_FULL);
  } else if (s_state == BL_STATE_FULL) {
    applyPwm(s_pwmFull);
  }

  Serial.printf("Backlight: timeout=%umin brightness=%u auto=%s\n",
                (unsigned)s_timeoutMin, (unsigned)s_brightness,
                s_autoEnabled ? "on" : "off");
}

void backlightNotifyActivity() {
  uint32_t now = millis();
  if (s_state == BL_STATE_OFF || s_state == BL_STATE_DIM) {
    s_wakeUntil = now + BL_WAKE_LATCH_MS;
  }
  s_lastActive = now;
  setState(BL_STATE_FULL);
}

bool backlightIsOn() {
  return s_state != BL_STATE_OFF;
}

bool backlightAutoEnabled() {
  return s_autoEnabled;
}

uint8_t backlightTimeoutMinutes() {
  return s_timeoutMin;
}

uint8_t backlightBrightness() {
  return s_brightness;
}

const char *backlightStatusText() {
  if (!s_autoEnabled) return "Backlight always on";
  switch (s_state) {
    case BL_STATE_DIM: return "Backlight dim";
    case BL_STATE_OFF: return "Backlight off";
    default:           return "Backlight on";
  }
}

void backlightSet(bool on) {
  if (on) backlightNotifyActivity();
  else setState(BL_STATE_OFF);
}

void backlightTick(uint32_t nowMs) {
  /* backlightTick already runs once per Director loop after LVGL, the desktop,
   * storage and communications have been initialised.  It is therefore a safe,
   * non-blocking home for the one-shot startup verification overlay. */
  gDirectorUnlockScreen.tick(nowMs, espNowReady, linkState, emergencyLocked);

  if (s_pin == 255) return;
  if (!s_autoEnabled) {
    if (s_state != BL_STATE_FULL) setState(BL_STATE_FULL);
    return;
  }

  if (nowMs < s_wakeUntil) {
    setState(BL_STATE_FULL);
    return;
  }

  const uint32_t offMs = (uint32_t)s_timeoutMin * 60000UL;
  uint32_t dimMs = offMs / 2UL;
  if (dimMs < 30000UL) dimMs = 30000UL;
  if (dimMs >= offMs) dimMs = (offMs > 1000UL) ? (offMs - 1000UL) : 0;

  uint32_t idle = nowMs - s_lastActive;
  if (idle >= offMs) {
    setState(BL_STATE_OFF);
  } else if (idle >= dimMs) {
    setState(BL_STATE_DIM);
  } else {
    setState(BL_STATE_FULL);
  }
}
