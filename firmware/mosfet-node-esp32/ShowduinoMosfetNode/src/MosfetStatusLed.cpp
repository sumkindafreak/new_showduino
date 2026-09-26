#include "MosfetStatusLed.h"
#include "MosfetNodeState.h"
#include "../BoardConfig.h"
#include <Arduino.h>

static bool sReady = false;
static uint32_t sLastToggle = 0;
static bool sOn = false;

void mosfetStatusLedBegin() {
#if SHOWDUINO_MOSFET_STATUS_LED_GPIO >= 0
  pinMode(SHOWDUINO_MOSFET_STATUS_LED_GPIO, OUTPUT);
  digitalWrite(SHOWDUINO_MOSFET_STATUS_LED_GPIO, LOW);
  sReady = true;
#if !SHOWDUINO_MOSFET_STATUS_LED_VERIFIED
  Serial.println("[MOSFET] Status LED GPIO23 SOFTWARE DEFINED / HARDWARE UNVERIFIED");
#endif
#endif
}

void mosfetStatusLedLoop() {
  if (!sReady) return;
  const ShowduinoMosfetNodeState st = mosfetNodeStateGet();
  uint32_t period = 800;
  bool solid = false;
  if (st == SHOWDUINO_MOSFET_ST_SHOW_CONTROLLED) {
    solid = true;
  } else if (st == SHOWDUINO_MOSFET_ST_EMERGENCY) {
    period = 120;
  } else if (st == SHOWDUINO_MOSFET_ST_FAULT) {
    period = 60;
  } else if (st == SHOWDUINO_MOSFET_ST_BOOTING) {
    period = 1000;
  } else {
    period = 400;
  }
  if (solid) {
    digitalWrite(SHOWDUINO_MOSFET_STATUS_LED_GPIO, HIGH);
    sOn = true;
    return;
  }
  const uint32_t now = millis();
  if ((now - sLastToggle) >= period) {
    sLastToggle = now;
    sOn = !sOn;
    digitalWrite(SHOWDUINO_MOSFET_STATUS_LED_GPIO, sOn ? HIGH : LOW);
  }
}
