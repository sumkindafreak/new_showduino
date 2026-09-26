#include "MosfetOutputEngine.h"
#include "../BoardConfig.h"

static ShowduinoMosfetEngine sEng;
static const int kPins[4] = {
  SHOWDUINO_MOSFET_OUT1_GPIO,
  SHOWDUINO_MOSFET_OUT2_GPIO,
  SHOWDUINO_MOSFET_OUT3_GPIO,
  SHOWDUINO_MOSFET_OUT4_GPIO
};
static bool sPwmReady = false;

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
  Serial.println("[MOSFET] Output engine ready (LEDC duty 0)");
}

void mosfetOutputEngineLoop() {
  showduino_mosfet_engine_tick(&sEng, millis());
}

void mosfetOutputEngineAllOff(const char *reason) {
  showduino_mosfet_engine_all_off(&sEng, reason ? reason : "ALL_OFF");
  Serial.printf("[MOSFET] ALL OFF (%s)\n", reason ? reason : "");
}

bool mosfetOutputEngineSetLevel(uint8_t ch, uint8_t level) {
  return showduino_mosfet_engine_set_level(&sEng, ch, level) != 0;
}
bool mosfetOutputEngineOn(uint8_t ch) {
  return showduino_mosfet_engine_on(&sEng, ch) != 0;
}
bool mosfetOutputEngineOff(uint8_t ch) {
  return showduino_mosfet_engine_off(&sEng, ch) != 0;
}
bool mosfetOutputEnginePulse(uint8_t ch, uint8_t level, uint32_t ms) {
  return showduino_mosfet_engine_pulse(&sEng, ch, level, ms, millis()) != 0;
}
bool mosfetOutputEngineFade(uint8_t ch, uint8_t level, uint32_t ms) {
  return showduino_mosfet_engine_fade(&sEng, ch, level, ms, millis()) != 0;
}
void mosfetOutputEngineLevels(uint8_t out[4]) {
  showduino_mosfet_engine_levels(&sEng, out);
}
