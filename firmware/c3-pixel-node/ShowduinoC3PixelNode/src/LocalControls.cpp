#include "LocalControls.h"
#include "../BoardConfig.h"

struct KeyMap {
  int pin;
  PixelLocalButton btn;
  const char *name;
};

static const KeyMap kKeys[] = {
  { SHOWDUINO_PIXEL_BTN_A, PIXEL_BTN_A, "BTN A BOOT/test" },
  { SHOWDUINO_PIXEL_BTN_B, PIXEL_BTN_B, "BTN B menu/page" },
};

static uint8_t sStable[2] = {1, 1};
static uint8_t sLast[2] = {1, 1};
static uint32_t sEdgeMs[2] = {0, 0};
static uint32_t sDownMs[2] = {0, 0};
static bool sLongSent[2] = {false, false};

void pixelLocalBegin() {
  for (size_t i = 0; i < 2; i++) {
    pinMode(kKeys[i].pin, INPUT_PULLUP);
    sStable[i] = digitalRead(kKeys[i].pin) ? 1 : 0;
    sLast[i] = sStable[i];
  }
}

PixelLocalEvent pixelLocalPoll() {
  PixelLocalEvent ev = { PIXEL_BTN_NONE, false };
  const uint32_t now = millis();
  for (size_t i = 0; i < 2; i++) {
    const uint8_t raw = digitalRead(kKeys[i].pin) ? 1 : 0;
    if (raw != sLast[i]) {
      sLast[i] = raw;
      sEdgeMs[i] = now;
    }
    if ((now - sEdgeMs[i]) < SHOWDUINO_PIXEL_KEY_DEBOUNCE_MS) continue;
    if (raw != sStable[i]) {
      sStable[i] = raw;
      if (raw == 0) {
        sDownMs[i] = now;
        sLongSent[i] = false;
      } else if (!sLongSent[i]) {
        ev.btn = kKeys[i].btn;
        ev.longPress = false;
        return ev;
      }
    } else if (raw == 0 && !sLongSent[i] &&
               (now - sDownMs[i]) >= SHOWDUINO_PIXEL_KEY_LONG_MS) {
      sLongSent[i] = true;
      ev.btn = kKeys[i].btn;
      ev.longPress = true;
      return ev;
    }
  }
  return ev;
}

void pixelLocalPrintStatus() {
  Serial.println("[BUTTONS] HUNT C3 OLED board");
  for (size_t i = 0; i < 2; i++) {
    Serial.printf("  %s GPIO%d %s\n", kKeys[i].name, kKeys[i].pin,
                  digitalRead(kKeys[i].pin) ? "UP" : "DOWN");
  }
  Serial.println("  Local A: line test (blocked in emergency / P4 show control / NOT INITIALISED)");
  Serial.println("  Local B: OLED page / long=STATUS dump");
}
