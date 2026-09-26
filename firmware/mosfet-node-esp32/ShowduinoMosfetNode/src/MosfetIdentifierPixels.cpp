#include "MosfetIdentifierPixels.h"
#include "MosfetOutputEngine.h"
#include "MosfetNodeState.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_mosfet_identifier.h"

#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel *sStrip = nullptr;
static bool sReady = false;
static bool sForcedOff = false;
static uint8_t sLastGreen[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static int8_t sLastIdentifyLit = -2; /* -2 unknown, -1 none, 0..3 lit */
static uint32_t sIdentifyUntil = 0;
static uint32_t sIdentifyStepMs = 0;
static uint8_t sIdentifyStep = 0;

void mosfetIdentifierPixelsSafeGpio() {
  pinMode(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO, OUTPUT);
  digitalWrite(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO, LOW);
}

static void pushGreenFrame(const uint8_t green[4], bool force) {
  if (!sReady || !sStrip) return;
  bool changed = force;
  for (uint8_t i = 0; i < 4; ++i) {
    if (sLastGreen[i] != green[i]) {
      changed = true;
      break;
    }
  }
  if (!changed) return;
  for (uint8_t i = 0; i < 4; ++i) {
    sLastGreen[i] = green[i];
    sStrip->setPixelColor(i, sStrip->Color(0, green[i], 0));
  }
  sLastIdentifyLit = -1;
  sStrip->show();
}

void mosfetIdentifierPixelsAllOff() {
  sIdentifyUntil = 0;
  sIdentifyStep = 0;
  sForcedOff = true;
  sLastIdentifyLit = -2;
  uint8_t zero[4] = {0, 0, 0, 0};
  if (sReady && sStrip) {
    pushGreenFrame(zero, true);
  } else {
    for (uint8_t i = 0; i < 4; ++i) sLastGreen[i] = 0;
  }
}

void mosfetIdentifierPixelsBegin() {
  sReady = false;
  sForcedOff = false;
  sIdentifyUntil = 0;
  sLastIdentifyLit = -2;
  mosfetIdentifierPixelsSafeGpio();
  sStrip = new Adafruit_NeoPixel(
      SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_COUNT,
      SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO,
      NEO_GRB + NEO_KHZ800);
  if (!sStrip) {
    Serial.println("[MOSFET] Identifier pixels unavailable (alloc fail) — outputs unaffected");
    return;
  }
  sStrip->begin();
  sStrip->setBrightness(255);
  sStrip->clear();
  sStrip->show();
  sReady = true;
  for (uint8_t i = 0; i < 4; ++i) sLastGreen[i] = 0;
#if !SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_VERIFIED
  Serial.println("[MOSFET] Identifier pixels GPIO25 SOFTWARE DEFINED / HARDWARE UNVERIFIED");
#endif
  Serial.printf("[MOSFET] Identifier pixels: %u × WS2812 on GPIO%u (max green %u)\n",
                (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_COUNT,
                (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO,
                (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS);
}

void mosfetIdentifierPixelsIdentify() {
  if (mosfetNodeStateEmergency()) {
    mosfetIdentifierPixelsAllOff();
    return;
  }
  sForcedOff = false;
  sIdentifyUntil = millis() + SHOWDUINO_MOSFET_IDENTIFY_MS;
  sIdentifyStepMs = millis();
  sIdentifyStep = 0;
  sLastIdentifyLit = -2;
  Serial.println("[MOSFET] IDENTIFY — local indicator chase (outputs unchanged)");
}

bool mosfetIdentifierPixelsReady() { return sReady; }
bool mosfetIdentifierPixelsIdentifying() {
  return sIdentifyUntil != 0 && (int32_t)(millis() - sIdentifyUntil) < 0;
}

void mosfetIdentifierPixelsService() {
  if (mosfetNodeStateEmergency()) {
    if (!sForcedOff || sIdentifyUntil) mosfetIdentifierPixelsAllOff();
    return;
  }

  if (sIdentifyUntil) {
    if ((int32_t)(millis() - sIdentifyUntil) >= 0) {
      sIdentifyUntil = 0;
      sForcedOff = false;
      sLastIdentifyLit = -2;
      for (uint8_t i = 0; i < 4; ++i) sLastGreen[i] = 0xFF; /* force refresh */
    } else if (sReady && sStrip) {
      const uint32_t now = millis();
      if ((now - sIdentifyStepMs) >= 200UL) {
        sIdentifyStepMs = now;
        sIdentifyStep = (uint8_t)((sIdentifyStep + 1) % 4);
      }
      if (sLastIdentifyLit != (int8_t)sIdentifyStep) {
        const uint8_t w = SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS;
        for (uint8_t i = 0; i < 4; ++i) {
          if (i == sIdentifyStep) sStrip->setPixelColor(i, sStrip->Color(w, w, w));
          else sStrip->setPixelColor(i, 0);
          sLastGreen[i] = 0xFF; /* invalidate green cache */
        }
        sLastIdentifyLit = (int8_t)sIdentifyStep;
        sStrip->show();
      }
      return;
    }
  }

  if (!sReady) return;
  sForcedOff = false;
  uint8_t levels[4] = {0, 0, 0, 0};
  uint8_t green[4] = {0, 0, 0, 0};
  mosfetOutputEngineLevels(levels);
  showduino_mosfet_identifier_frame(levels, SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS, green);
  pushGreenFrame(green, false);
}
