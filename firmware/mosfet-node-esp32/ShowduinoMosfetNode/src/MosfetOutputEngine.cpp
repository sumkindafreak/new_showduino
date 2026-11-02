#include "MosfetOutputEngine.h"
#include "MosfetIdentifierPixels.h"
#include "../BoardConfig.h"
#include "MosfetRecoveryStore.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static ShowduinoMosfetEngine sEng;
static const int kPins[4] = {
  SHOWDUINO_MOSFET_OUT1_GPIO,
  SHOWDUINO_MOSFET_OUT2_GPIO,
  SHOWDUINO_MOSFET_OUT3_GPIO,
  SHOWDUINO_MOSFET_OUT4_GPIO
};
static bool sPwmReady = false;
static SemaphoreHandle_t sEngineMutex = nullptr;
bool mosfetOutputStateTake() {
  return sEngineMutex && xSemaphoreTakeRecursive(sEngineMutex, portMAX_DELAY) == pdTRUE;
}
void mosfetOutputStateGive() { xSemaphoreGiveRecursive(sEngineMutex); }
// ESP-NOW callbacks and loop() may run on different tasks. Keep output state,
// recovery snapshots and flash commits ordered as one operation.
class EngineLock {
 public:
  EngineLock() : locked_(mosfetOutputStateTake()) {}
  ~EngineLock() { if (locked_) mosfetOutputStateGive(); }
  EngineLock(const EngineLock&) = delete;
  EngineLock& operator=(const EngineLock&) = delete;
  explicit operator bool() const { return locked_; }
 private:
  bool locked_;
};
static uint8_t sSteady[4] = {};
static uint8_t sRestoreMask = 0, sPausedMask = 0;
static ShowduinoMosfetChannel sPaused[4];
static bool sSavePending = false;
static uint32_t sSaveRetryMs = 0;

static void saveSteady() {
  sSavePending = !mosfetRecoverySave(sSteady);
  sSaveRetryMs = millis();
}
static void replaceChannel(uint8_t channel, uint8_t steady) {
  const uint8_t bit = (uint8_t)(1U << (channel - 1U));
  sPausedMask &= (uint8_t)~bit;
  sRestoreMask &= (uint8_t)~bit;
  sSteady[channel - 1U] = steady;
  saveSteady();
}

static void writeDuty(uint8_t idx, uint8_t pct) {
  if (idx >= 4) return;
  const uint32_t maxDuty = (1u << SHOWDUINO_MOSFET_PWM_BITS) - 1u;
  uint32_t duty = (pct >= 100) ? maxDuty : ((uint32_t)pct * maxDuty) / 100u;
  if (!sPwmReady) {
    digitalWrite(kPins[idx], duty ? HIGH : LOW);
    return;
  }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(kPins[idx], duty);
#else
  ledcWrite(idx, duty);
#endif
}

void mosfetOutputEngineSafeGpioFirst() {
  for (int i = 0; i < 4; ++i) {
    pinMode(kPins[i], OUTPUT);
    digitalWrite(kPins[i], LOW);
  }
}

void mosfetOutputEngineBegin() {
  mosfetOutputEngineSafeGpioFirst();
  if (!sEngineMutex) sEngineMutex = xSemaphoreCreateRecursiveMutex();
  EngineLock lock;
  if (!lock) {
    Serial.println("[MOSFET][ERROR] Output synchronization unavailable; outputs stay off");
    return;
  }
  for (int i = 0; i < 4; ++i) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(kPins[i], SHOWDUINO_MOSFET_PWM_HZ, SHOWDUINO_MOSFET_PWM_BITS);
    ledcWrite(kPins[i], 0);
#else
    ledcSetup(i, SHOWDUINO_MOSFET_PWM_HZ, SHOWDUINO_MOSFET_PWM_BITS);
    ledcAttachPin(kPins[i], i);
    ledcWrite(i, 0);
#endif
  }
  sPwmReady = true;
  showduino_mosfet_engine_begin(&sEng, writeDuty);
  sPausedMask = sRestoreMask = 0;
  sSavePending = false;
  mosfetRecoveryLoad(sSteady);
  for (uint8_t i = 0; i < 4; ++i) if (sSteady[i]) sRestoreMask |= (uint8_t)(1U << i);
  Serial.println("[MOSFET] Output engine ready (LEDC duty 0)");
}

void mosfetOutputEngineLoop() {
  EngineLock lock;
  if (!lock) return;
  ShowduinoMosfetChMode before[4];
  for (uint8_t i = 0; i < 4; ++i) before[i] = sEng.ch[i].mode;
  showduino_mosfet_engine_tick(&sEng, millis());
  bool changed = false;
  for (uint8_t i = 0; i < 4; ++i) {
    if (before[i] == SHOWDUINO_MOSFET_MODE_FADE &&
        sEng.ch[i].mode != SHOWDUINO_MOSFET_MODE_FADE) {
      sSteady[i] = sEng.ch[i].level;
      changed = true;
    }
  }
  if (changed || (sSavePending && millis() - sSaveRetryMs >= 1000U)) saveSteady();
}

void mosfetOutputEngineAllOff(const char *reason) {
  EngineLock lock;
  if (!lock) { mosfetOutputEngineSafeGpioFirst(); return; }
  sPausedMask = sRestoreMask = 0;
  showduino_mosfet_engine_all_off(&sEng, reason ? reason : "ALL_OFF");
  memset(sSteady, 0, sizeof(sSteady));
  saveSteady();
  mosfetIdentifierPixelsAllOff();
  Serial.printf("[MOSFET] ALL OFF (%s)\n", reason ? reason : "");
}

bool mosfetOutputEngineSetLevel(uint8_t ch, uint8_t level) {
  EngineLock lock;
  if (!lock) return false;
  const bool ok = showduino_mosfet_engine_set_level(&sEng, ch, level) != 0;
  if (ok) replaceChannel(ch, level);
  return ok;
}
bool mosfetOutputEngineOn(uint8_t ch) {
  return mosfetOutputEngineSetLevel(ch, 100);
}
bool mosfetOutputEngineOff(uint8_t ch) {
  return mosfetOutputEngineSetLevel(ch, 0);
}
bool mosfetOutputEnginePulse(uint8_t ch, uint8_t level, uint32_t ms) {
  EngineLock lock;
  if (!lock) return false;
  const bool ok = showduino_mosfet_engine_pulse(&sEng, ch, level, ms, millis()) != 0;
  if (ok) replaceChannel(ch, 0); // Timed effects are never restored after node power loss.
  return ok;
}
bool mosfetOutputEngineFade(uint8_t ch, uint8_t level, uint32_t ms) {
  EngineLock lock;
  if (!lock) return false;
  const bool ok = showduino_mosfet_engine_fade(&sEng, ch, level, ms, millis()) != 0;
  if (ok) replaceChannel(ch, ms ? 0 : level);
  return ok;
}
void mosfetOutputEngineLevels(uint8_t out[4]) {
  if (!out) return;
  EngineLock lock;
  if (!lock) { memset(out, 0, 4); return; }
  showduino_mosfet_engine_levels(&sEng, out);
}

void mosfetOutputEnginePauseForControlLoss() {
  EngineLock lock;
  if (!lock) return;
  const uint32_t now = millis();
  // Update to the loss-detection instant, then hold physical levels unchanged.
  mosfetOutputEngineLoop();
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t bit = (uint8_t)(1U << i);
    if (sPausedMask & bit) continue;
    auto& channel = sEng.ch[i];
    if (channel.mode != SHOWDUINO_MOSFET_MODE_PULSE && channel.mode != SHOWDUINO_MOSFET_MODE_FADE) continue;
    sPaused[i] = channel;
    if (channel.mode == SHOWDUINO_MOSFET_MODE_PULSE) sPaused[i].pulseDeadlineMs -= now;
    else sPaused[i].fadeStartMs = now - channel.fadeStartMs;
    sPausedMask |= bit;
    channel.mode = channel.level ? SHOWDUINO_MOSFET_MODE_HOLD : SHOWDUINO_MOSFET_MODE_OFF;
  }
  Serial.println("[MOSFET] P4 control lost: levels held, timed effects paused");
}

void mosfetOutputEngineResumeOnGrant() {
  EngineLock lock;
  if (!lock) return;
  const uint32_t now = millis();
  for (uint8_t i = 0; i < 4; ++i) {
    const uint8_t bit = (uint8_t)(1U << i);
    if (sRestoreMask & bit) showduino_mosfet_engine_set_level(&sEng, i + 1U, sSteady[i]);
    if (sPausedMask & bit) {
      sEng.ch[i] = sPaused[i];
      if (sEng.ch[i].mode == SHOWDUINO_MOSFET_MODE_PULSE) sEng.ch[i].pulseDeadlineMs += now;
      else sEng.ch[i].fadeStartMs = now - sEng.ch[i].fadeStartMs;
    }
  }
  sRestoreMask = sPausedMask = 0;
  Serial.println("[MOSFET] P4 control granted: steady levels retained/restored, paused effects resumed");
}
