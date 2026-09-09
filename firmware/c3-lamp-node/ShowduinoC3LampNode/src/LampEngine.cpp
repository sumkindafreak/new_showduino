#include "LampEngine.h"
#include "LampEffects.h"
#include "../BoardConfig.h"

static Adafruit_NeoPixel sStrip(
    SHOWDUINO_LAMP_PIXEL_COUNT,
    SHOWDUINO_LAMP_PIXEL_PIN,
    SHOWDUINO_LAMP_PIXEL_ORDER);

static bool sReady = false;
static bool sEmergency = false;
static bool sActive = false;
static uint8_t sBri = SHOWDUINO_LAMP_DEFAULT_BRIGHTNESS;
static ShowduinoLampFx sFx = SHOWDUINO_LAMP_FX_CARBIDE_FLAME;
static uint8_t sSpeed = 50;
static uint8_t sIntensity = 50;
static uint8_t sR = 255, sG = 180, sB = 40;
static uint32_t sLastTick = 0;

bool lampEngineBegin() {
  pinMode(SHOWDUINO_LAMP_PIXEL_PIN, OUTPUT);
  sStrip.begin();
  sStrip.clear();
  sStrip.show();
  sReady = true;
  sEmergency = false;
  sActive = false;
  lampEffectsReset(sFx);
  return true;
}

static uint16_t intervalMs() {
  const ShowduinoLampFxInfo *info = showduino_lamp_fx_info(sFx);
  uint16_t base = info ? info->intervalMs : 40;
  if (sSpeed >= 1 && sSpeed <= 100 &&
      showduino_lamp_fx_info(sFx) &&
      showduino_lamp_fx_info(sFx)->origin == SHOWDUINO_LAMP_FX_SHOWDUINO) {
    /* Speed only scales extra Showduino FX, not original carbide timing. */
    base = (uint16_t)((uint32_t)base * (50u + (100u - sSpeed)) / 100u);
    if (base < 15) base = 15;
  }
  return base;
}

void lampEngineService() {
  if (!sReady) return;
  const uint32_t now = millis();
  if (sEmergency) {
    if (now - sLastTick < 80) return;
    sLastTick = now;
    for (uint16_t i = 0; i < sStrip.numPixels(); ++i) {
      sStrip.setPixelColor(i, sStrip.Color(255, 255, 255));
    }
    sStrip.show();
    return;
  }
  if (!sActive) return;
  if (now - sLastTick < intervalMs()) return;
  sLastTick = now;
  lampEffectsTick(sStrip, sFx, sBri, sSpeed, sIntensity, sR, sG, sB, now);
}

void lampEngineOff() {
  sActive = false;
  if (!sEmergency) {
    sStrip.clear();
    sStrip.show();
  }
}

void lampEngineSetBrightness(uint8_t bri0to100) {
  sBri = showduino_lamp_clamp_bri(bri0to100);
}

uint8_t lampEngineBrightness() { return sBri; }

void lampEngineSetSolid(uint8_t r, uint8_t g, uint8_t b) {
  sR = r;
  sG = g;
  sB = b;
  sFx = SHOWDUINO_LAMP_FX_SOLID;
  lampEffectsReset(sFx);
  sActive = true;
}

void lampEngineSetFx(ShowduinoLampFx fx, uint8_t speed, uint8_t intensity) {
  sFx = fx;
  if (speed >= 1 && speed <= 100) sSpeed = speed;
  if (intensity <= 100) sIntensity = intensity;
  lampEffectsReset(fx);
  sActive = true;
}

ShowduinoLampFx lampEngineFx() { return sFx; }

const char *lampEngineFxDisplay() {
  const ShowduinoLampFxInfo *info = showduino_lamp_fx_info(sFx);
  return info ? info->display : "Unknown";
}

const char *lampEngineFxToken() {
  const ShowduinoLampFxInfo *info = showduino_lamp_fx_info(sFx);
  return info ? info->token : "UNKNOWN";
}

bool lampEngineActive() { return sActive && !sEmergency; }

void lampEngineOnEmergency(bool active) {
  sEmergency = active;
  if (active) {
    sActive = false;
    for (uint16_t i = 0; i < sStrip.numPixels(); ++i) {
      sStrip.setPixelColor(i, sStrip.Color(255, 255, 255));
    }
    sStrip.show();
  } else {
    sStrip.clear();
    sStrip.show();
  }
}

bool lampEngineEmergency() { return sEmergency; }

void lampEngineTest() {
  if (sEmergency) return;
  lampEngineSetFx(SHOWDUINO_LAMP_FX_CARBIDE_FLAME, 0, 255);
}

void lampEngineFill(uint8_t r, uint8_t g, uint8_t b) {
  for (uint16_t i = 0; i < sStrip.numPixels(); ++i) {
    sStrip.setPixelColor(i, sStrip.Color(r, g, b));
  }
  sStrip.show();
}

uint16_t lampEngineCount() { return SHOWDUINO_LAMP_PIXEL_COUNT; }
int lampEnginePin() { return SHOWDUINO_LAMP_PIXEL_PIN; }
