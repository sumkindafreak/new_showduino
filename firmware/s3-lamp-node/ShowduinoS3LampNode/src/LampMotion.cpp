#include "LampMotion.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Arduino.h>

static ShowduinoMotionDetector sDet;
static ShowduinoMotionEvent sPending = SHOWDUINO_MOTION_EV_NONE;

static void applyPinMode() {
  if (SHOWDUINO_LAMP_MOTION_PIN < 0) return;
  if (lampConfigMotionActiveLow()) {
    pinMode(SHOWDUINO_LAMP_MOTION_PIN, INPUT_PULLUP);
  } else {
    pinMode(SHOWDUINO_LAMP_MOTION_PIN, INPUT_PULLDOWN);
  }
}

void lampMotionBegin() {
  sPending = SHOWDUINO_MOTION_EV_NONE;
  showduino_motion_reset(&sDet, SHOWDUINO_LAMP_MOTION_DEBOUNCE_MS,
                         lampConfigMotionCooldownMs());
  if (SHOWDUINO_LAMP_MOTION_PIN < 0) {
    Serial.println("[MOTION] GPIO UNCONFIRMED");
    return;
  }
  applyPinMode();
  Serial.printf("[MOTION] GPIO%d digital 3.3V-only polarity=%s action=%s %s\n",
                SHOWDUINO_LAMP_MOTION_PIN,
                showduino_motion_polarity_name(lampConfigMotionActiveLow()),
                showduino_motion_action_name(lampConfigMotionAction()),
                SHOWDUINO_LAMP_MOTION_PIN_CONFIRMED ? "PHYSICAL"
                                                    : "PIN ASSIGNED, SENSOR UNCONFIRMED");
}

void lampMotionApplyConfig() {
  showduino_motion_reset(&sDet, SHOWDUINO_LAMP_MOTION_DEBOUNCE_MS,
                         lampConfigMotionCooldownMs());
  sPending = SHOWDUINO_MOTION_EV_NONE;
  applyPinMode();
}

void lampMotionService() {
  if (SHOWDUINO_LAMP_MOTION_PIN < 0) return;
  const uint8_t raw = digitalRead(SHOWDUINO_LAMP_MOTION_PIN) ? 1 : 0;
  const ShowduinoMotionEvent ev =
      showduino_motion_feed(&sDet, raw, lampConfigMotionActiveLow(), millis());
  if (ev != SHOWDUINO_MOTION_EV_NONE) sPending = ev;
}

ShowduinoMotionEvent lampMotionTakeEvent() {
  const ShowduinoMotionEvent ev = sPending;
  sPending = SHOWDUINO_MOTION_EV_NONE;
  return ev;
}

const ShowduinoMotionDetector *lampMotionDetector() { return &sDet; }
int lampMotionPin() { return SHOWDUINO_LAMP_MOTION_PIN; }
bool lampMotionPinAssigned() { return SHOWDUINO_LAMP_MOTION_PIN >= 0; }
bool lampMotionHardwareConfirmed() { return SHOWDUINO_LAMP_MOTION_PIN_CONFIRMED != 0; }
uint8_t lampMotionRawHigh() { return sDet.rawHigh; }
uint8_t lampMotionActive() { return sDet.stable; }
const char *lampMotionRawName() { return sDet.rawHigh ? "HIGH" : "LOW"; }
const char *lampMotionStateName() { return sDet.stable ? "ACTIVE" : "CLEAR"; }

const char *lampMotionPhysicalStatus() {
  if (SHOWDUINO_LAMP_MOTION_PIN < 0) return "UNCONFIRMED";
  return SHOWDUINO_LAMP_MOTION_PIN_CONFIRMED ? "PHYSICAL" : "ASSIGNED_UNCONFIRMED";
}
