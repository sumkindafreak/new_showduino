#include "LocalButtons.h"
#include "../BoardConfig.h"

struct KeyMap {
  int pin;
  ShowduinoAudioButton btn;
  const char *name;
  bool inputOnly;
};

static const KeyMap kKeys[] = {
  { SHOWDUINO_AUDIO_KEY1, SHOWDUINO_AUDIO_BTN_PLAY,     "KEY1 PLAY",  true },
  { SHOWDUINO_AUDIO_KEY2, SHOWDUINO_AUDIO_BTN_MODE,     "KEY2 MODE",  false },
  { SHOWDUINO_AUDIO_KEY3, SHOWDUINO_AUDIO_BTN_VOL_DOWN, "KEY3 VOL-",  false },
  { SHOWDUINO_AUDIO_KEY4, SHOWDUINO_AUDIO_BTN_VOL_UP,   "KEY4 VOL+",  false },
  { SHOWDUINO_AUDIO_KEY5, SHOWDUINO_AUDIO_BTN_PREV,     "KEY5 PREV",  false },
  { SHOWDUINO_AUDIO_KEY6, SHOWDUINO_AUDIO_BTN_NEXT,     "KEY6 NEXT",  false },
};

static uint8_t sStable[6] = {1, 1, 1, 1, 1, 1};
static uint8_t sLast[6] = {1, 1, 1, 1, 1, 1};
static uint32_t sEdgeMs[6] = {0};
static uint32_t sDownMs[6] = {0};
static bool sLongSent[6] = {false};

void localButtonsBegin() {
  for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
    if (kKeys[i].pin < 0) continue;
    pinMode(kKeys[i].pin, kKeys[i].inputOnly ? INPUT : INPUT_PULLUP);
    sStable[i] = digitalRead(kKeys[i].pin) ? 1 : 0;
    sLast[i] = sStable[i];
  }
}

LocalButtonEvent localButtonsPoll() {
  LocalButtonEvent ev = { SHOWDUINO_AUDIO_BTN_NONE, false };
  const uint32_t now = millis();
  for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
    if (kKeys[i].pin < 0) continue;
    const uint8_t raw = digitalRead(kKeys[i].pin) ? 1 : 0;
    if (raw != sLast[i]) {
      sLast[i] = raw;
      sEdgeMs[i] = now;
    }
    if ((now - sEdgeMs[i]) < 40) continue;
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
               (now - sDownMs[i]) >= SHOWDUINO_AUDIO_KEY_LONG_MS) {
      sLongSent[i] = true;
      ev.btn = kKeys[i].btn;
      ev.longPress = true;
      return ev;
    }
  }
  return ev;
}

void localButtonsPrintStatus() {
  Serial.println("[BUTTONS] A161 commissioning map");
  for (size_t i = 0; i < sizeof(kKeys) / sizeof(kKeys[0]); i++) {
    if (kKeys[i].pin < 0) {
      Serial.printf("  %s disabled (shared pin)\n", kKeys[i].name);
      continue;
    }
    Serial.printf("  %s GPIO%d %s\n", kKeys[i].name, kKeys[i].pin,
                  digitalRead(kKeys[i].pin) ? "UP" : "DOWN");
  }
  Serial.println("  KEY7 not present (BOOT=GPIO0/MCLK, RST=hardware)");
}
