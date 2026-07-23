#include "EmergencyPixels.h"

#if SHOWDUINO_EMERGENCY_PIXEL_ENABLED

#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel *sStrip = nullptr;
static bool sReady = false;

bool emergencyPixelsBegin() {
  if (sReady && sStrip) return true;

  sStrip = new Adafruit_NeoPixel(SHOWDUINO_EMERGENCY_PIXEL_COUNT,
                                 SHOWDUINO_EMERGENCY_PIXEL_PIN,
                                 NEO_GRB + NEO_KHZ800);
  if (!sStrip) {
    Serial.println("[Pixel] Emergency strip alloc failed");
    return false;
  }

  sStrip->begin();
  sStrip->setBrightness(SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS);
  sStrip->clear();
  sStrip->show();
  sReady = true;

  Serial.printf("[Pixel] Emergency line: pin=%d count=%u brightness=%u\n",
                SHOWDUINO_EMERGENCY_PIXEL_PIN,
                (unsigned)SHOWDUINO_EMERGENCY_PIXEL_COUNT,
                (unsigned)SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS);
  return true;
}

void emergencyPixelsSetWhite() {
  if (!sReady && !emergencyPixelsBegin()) return;
  if (!sStrip) return;

  const uint32_t white = sStrip->Color(255, 255, 255);
  for (uint16_t i = 0; i < sStrip->numPixels(); i++) {
    sStrip->setPixelColor(i, white);
  }
  sStrip->show();
  Serial.println("[Pixel] EMERGENCY → white");
}

void emergencyPixelsBlackout() {
  if (!sReady || !sStrip) return;
  sStrip->clear();
  sStrip->show();
  Serial.println("[Pixel] EMERGENCY clear → blackout");
}

bool emergencyPixelsReady() {
  return sReady;
}

#endif
