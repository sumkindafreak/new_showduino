#include "LocalControls.h"
#include "../BoardConfig.h"

static uint8_t sStable = 1;
static uint8_t sLast = 1;
static uint32_t sEdgeMs = 0;
static uint32_t sDownMs = 0;
static bool sLongSent = false;

void lampLocalBegin() {
  if (SHOWDUINO_LAMP_BTN_IGNITE < 0) {
    Serial.println("[BUTTON] ignition GPIO UNCONFIRMED");
    return;
  }
  pinMode(SHOWDUINO_LAMP_BTN_IGNITE, INPUT_PULLUP);
  sStable = digitalRead(SHOWDUINO_LAMP_BTN_IGNITE) ? 1 : 0;
  sLast = sStable;
}

LampLocalEvent lampLocalPoll() {
  LampLocalEvent ev = { LAMP_BTN_NONE, false };
  if (SHOWDUINO_LAMP_BTN_IGNITE < 0) return ev;
  const uint32_t now = millis();
  uint8_t raw = digitalRead(SHOWDUINO_LAMP_BTN_IGNITE) ? 1 : 0;
#if SHOWDUINO_LAMP_BTN_ACTIVE_LOW
  const uint8_t pressed = 0;
#else
  const uint8_t pressed = 1;
#endif
  if (raw != sLast) {
    sLast = raw;
    sEdgeMs = now;
  }
  if ((now - sEdgeMs) < SHOWDUINO_LAMP_KEY_DEBOUNCE_MS) return ev;
  if (raw != sStable) {
    sStable = raw;
    if (raw == pressed) {
      sDownMs = now;
      sLongSent = false;
    } else if (!sLongSent) {
      ev.btn = LAMP_BTN_IGNITE;
      ev.longPress = false;
    }
  } else if (raw == pressed && !sLongSent &&
             (now - sDownMs) >= SHOWDUINO_LAMP_KEY_LONG_MS) {
    sLongSent = true;
    ev.btn = LAMP_BTN_IGNITE;
    ev.longPress = true;
  }
  return ev;
}

void lampLocalPrintStatus() {
  Serial.println("[BUTTON] physical carbide striker");
  if (SHOWDUINO_LAMP_BTN_IGNITE < 0) {
    Serial.println("  GPIO UNCONFIRMED");
    return;
  }
  Serial.printf("  GPIO%d %s %s\n", SHOWDUINO_LAMP_BTN_IGNITE,
                SHOWDUINO_LAMP_BTN_POLARITY_NOTE,
                digitalRead(SHOWDUINO_LAMP_BTN_IGNITE) ? "HIGH" : "LOW");
}

bool lampLocalPinConfirmed() { return SHOWDUINO_LAMP_BTN_IGNITE >= 0; }
