#include "LampEffects.h"
#include "../BoardConfig.h"
#include <math.h>
#include <string.h>

struct FxState {
  float windPhase;
  float breathe;
  int leakPixel;
  uint32_t lastLeak;
  int beamPosition;
  int beamDir;
  uint32_t lastMove;
  float dimmer;
  uint32_t lastSputter;
  bool isLit;
  bool warning;
  uint32_t lastBlink;
  float glowBrightness;
  float glowDir;
  int hue;
  bool strobeOn;
  uint32_t lastToggle;
  bool redPhase;
  float fade;
  float pulse;
  uint32_t lastShow;
};

static FxState s;
static bool sNeedShow = true;

static uint8_t scale(uint8_t v, uint8_t bri) {
  return (uint8_t)((uint16_t)v * (uint16_t)bri / 100u);
}

static void put(Adafruit_NeoPixel &strip, int i, uint8_t r, uint8_t g, uint8_t b, uint8_t bri) {
  if (i < 0 || i >= (int)strip.numPixels()) return;
  strip.setPixelColor((uint16_t)i, strip.Color(scale(r, bri), scale(g, bri), scale(b, bri)));
}

static void fillAll(Adafruit_NeoPixel &strip, uint8_t r, uint8_t g, uint8_t b, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) put(strip, i, r, g, b, bri);
}

void lampEffectsReset(ShowduinoLampFx fx) {
  (void)fx;
  memset(&s, 0, sizeof(s));
  s.beamPosition = SHOWDUINO_LAMP_PIXEL_COUNT / 2;
  s.beamDir = 1;
  s.isLit = true;
  s.glowBrightness = 30;
  s.glowDir = 3;
}

static void carbideFlame(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    int flameBrightness = random(200, 255);
    put(strip, i, (uint8_t)flameBrightness, (uint8_t)flameBrightness,
        (uint8_t)(flameBrightness - 50), bri);
  }
}

static void carbideFlutter(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    float wind = sin(s.windPhase + i * 0.3f) * 0.3f + 0.7f;
    int brightLevel = (int)(wind * 255.0f);
    put(strip, i, (uint8_t)brightLevel, (uint8_t)brightLevel,
        (uint8_t)(brightLevel - 30), bri);
  }
  s.windPhase += 0.2f;
  if (s.windPhase >= 6.28f) s.windPhase = 0;
}

static void minerFlame(Adafruit_NeoPixel &strip, uint8_t bri) {
  s.breathe += 0.05f;
  float brightLevel = (sin(s.breathe) * 0.1f + 0.9f) * 255.0f;
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    put(strip, i, (uint8_t)brightLevel, (uint8_t)(brightLevel * 0.9f),
        (uint8_t)(brightLevel * 0.7f), bri);
  }
}

static void gasLeak(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastLeak < 150) {
    sNeedShow = false;
    return;
  }
  strip.clear();
  put(strip, s.leakPixel, 0, 30, 80, bri);
  if (random(100) < 20) {
    put(strip, (s.leakPixel + 1) % (int)strip.numPixels(), 0, 20, 60, bri);
  }
  s.leakPixel = (s.leakPixel + 1) % (int)strip.numPixels();
  s.lastLeak = now;
}

static void headlampBeam(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  /* Original carbide firmware never advanced beamPosition; keep that look. */
  (void)s.beamDir;
  if (now - s.lastMove < 300) {
    sNeedShow = false;
    return;
  }
  strip.clear();
  put(strip, s.beamPosition, 255, 255, 200, bri);
  if (s.beamPosition > 0) put(strip, s.beamPosition - 1, 100, 100, 80, bri);
  if (s.beamPosition < (int)strip.numPixels() - 1) {
    put(strip, s.beamPosition + 1, 100, 100, 80, bri);
  }
  s.lastMove = now;
}

static void oldFlame(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    int flicker = random(180, 255);
    put(strip, i, (uint8_t)flicker, (uint8_t)(flicker * 0.8f),
        (uint8_t)(flicker * 0.3f), bri);
  }
}

static void caveExplorer(Adafruit_NeoPixel &strip, uint8_t bri) {
  s.dimmer += 0.03f;
  float caveBright = (sin(s.dimmer) * 0.2f + 0.8f) * 120.0f;
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    put(strip, i, (uint8_t)caveBright, (uint8_t)(caveBright * 0.9f),
        (uint8_t)(caveBright * 0.6f), bri);
  }
}

static void lowFuel(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastSputter < (uint32_t)random(100, 800)) {
    sNeedShow = false;
    return;
  }
  if (s.isLit) {
    for (int i = 0; i < (int)strip.numPixels(); ++i) {
      int weak = random(50, 120);
      put(strip, i, (uint8_t)weak, (uint8_t)(weak * 0.8f),
          (uint8_t)(weak * 0.5f), bri);
    }
  } else {
    strip.clear();
  }
  s.isLit = !s.isLit;
  s.lastSputter = now;
}

static void canaryWarning(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  const uint32_t gap = s.warning ? 200 : 500;
  if (now - s.lastBlink < gap) {
    sNeedShow = false;
    return;
  }
  if (s.warning) fillAll(strip, 255, 255, 0, bri);
  else strip.clear();
  s.warning = !s.warning;
  s.lastBlink = now;
}

static void carbideFlicker(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    int flicker = random(100, 255);
    put(strip, i, (uint8_t)flicker, (uint8_t)random(30, 80), 0, bri);
  }
}

static void glow(Adafruit_NeoPixel &strip, uint8_t bri) {
  s.glowBrightness += s.glowDir;
  if (s.glowBrightness >= 255 || s.glowBrightness <= 30) s.glowDir *= -1;
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    put(strip, i, (uint8_t)((255 * s.glowBrightness) / 255),
        (uint8_t)((140 * s.glowBrightness) / 255), 0, bri);
  }
}

static void colorShift(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    float h = (s.hue % 360) / 60.0f;
    int c = 150;
    float x = c * (1 - fabsf(fmodf(h, 2) - 1));
    uint8_t r, g, b;
    if (h < 1) { r = (uint8_t)c; g = (uint8_t)x; b = 0; }
    else if (h < 2) { r = (uint8_t)x; g = (uint8_t)c; b = 0; }
    else if (h < 3) { r = 0; g = (uint8_t)c; b = (uint8_t)x; }
    else if (h < 4) { r = 0; g = (uint8_t)x; b = (uint8_t)c; }
    else if (h < 5) { r = (uint8_t)x; g = 0; b = (uint8_t)c; }
    else { r = (uint8_t)c; g = 0; b = (uint8_t)x; }
    put(strip, i, r, g, b, bri);
  }
  s.hue += 200;
  if (s.hue >= 360) s.hue = 0;
}

static void strobe(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastToggle < 100) {
    sNeedShow = false;
    return;
  }
  if (s.strobeOn) fillAll(strip, 255, 255, 255, bri);
  else strip.clear();
  s.strobeOn = !s.strobeOn;
  s.lastToggle = now;
}

static void policeStrobe(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastToggle < 200) {
    sNeedShow = false;
    return;
  }
  if (s.redPhase) fillAll(strip, 255, 0, 0, bri);
  else fillAll(strip, 0, 0, 255, bri);
  s.redPhase = !s.redPhase;
  s.lastToggle = now;
}

/* --- Additional Showduino FX. Do not change carbide implementations above. --- */

static void extraSolid(Adafruit_NeoPixel &strip, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  fillAll(strip, r, g, b, bri);
}

static void extraFadeIn(Adafruit_NeoPixel &strip, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  s.fade += 0.02f;
  if (s.fade > 1) s.fade = 1;
  const uint8_t m = (uint8_t)(bri * s.fade);
  fillAll(strip, r, g, b, m);
}

static void extraFadeOut(Adafruit_NeoPixel &strip, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  if (s.fade <= 0) s.fade = 1;
  s.fade -= 0.02f;
  if (s.fade < 0) s.fade = 0;
  fillAll(strip, r, g, b, (uint8_t)(bri * s.fade));
}

static void extraPulse(Adafruit_NeoPixel &strip, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  s.pulse += 0.12f;
  float w = (sinf(s.pulse) * 0.5f + 0.5f);
  fillAll(strip, r, g, b, (uint8_t)(bri * w));
}

static void extraBreathe(Adafruit_NeoPixel &strip, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  s.pulse += 0.04f;
  float w = (sinf(s.pulse) * 0.35f + 0.65f);
  fillAll(strip, r, g, b, (uint8_t)(bri * w));
}

static void extraCandle(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    int v = random(90, 180);
    put(strip, i, (uint8_t)v, (uint8_t)(v * 0.55f), 8, bri);
  }
}

static void extraFire(Adafruit_NeoPixel &strip, uint8_t bri) {
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    int v = random(120, 255);
    put(strip, i, (uint8_t)v, (uint8_t)random(20, 80), 0, bri);
  }
}

static void extraRandomFlicker(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastShow < (uint32_t)random(30, 120)) {
    sNeedShow = false;
    return;
  }
  s.lastShow = now;
  for (int i = 0; i < (int)strip.numPixels(); ++i) {
    if (random(100) < 35) put(strip, i, 255, 180, 40, bri);
    else put(strip, i, 20, 8, 0, bri);
  }
}

static void extraFault(Adafruit_NeoPixel &strip, uint8_t bri, uint32_t now) {
  if (now - s.lastToggle < 80) {
    sNeedShow = false;
    return;
  }
  s.lastToggle = now;
  s.strobeOn = !s.strobeOn;
  if (s.strobeOn) fillAll(strip, 255, 0, 0, bri);
  else strip.clear();
}

void lampEffectsTick(Adafruit_NeoPixel &strip, ShowduinoLampFx fx,
                     uint8_t bri0to100, uint8_t speed, uint8_t intensity,
                     uint8_t solidR, uint8_t solidG, uint8_t solidB,
                     uint32_t nowMs) {
  (void)speed;
  (void)intensity;
  uint8_t bri = bri0to100;
  if (bri > 100) bri = 100;
  sNeedShow = true;

  switch (fx) {
    case SHOWDUINO_LAMP_FX_CARBIDE_FLAME: carbideFlame(strip, bri); break;
    case SHOWDUINO_LAMP_FX_CARBIDE_FLUTTER: carbideFlutter(strip, bri); break;
    case SHOWDUINO_LAMP_FX_MINER_FLAME: minerFlame(strip, bri); break;
    case SHOWDUINO_LAMP_FX_GAS_LEAK: gasLeak(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_HEADLAMP_BEAM: headlampBeam(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_OLD_FLAME: oldFlame(strip, bri); break;
    case SHOWDUINO_LAMP_FX_CAVE_EXPLORER: caveExplorer(strip, bri); break;
    case SHOWDUINO_LAMP_FX_LOW_FUEL: lowFuel(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_CANARY_WARNING: canaryWarning(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_FLICKER: carbideFlicker(strip, bri); break;
    case SHOWDUINO_LAMP_FX_GLOW: glow(strip, bri); break;
    case SHOWDUINO_LAMP_FX_COLOR_SHIFT: colorShift(strip, bri); break;
    case SHOWDUINO_LAMP_FX_STROBE: strobe(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_POLICE_STROBE: policeStrobe(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_SOLID: extraSolid(strip, bri, solidR, solidG, solidB); break;
    case SHOWDUINO_LAMP_FX_FADE_IN: extraFadeIn(strip, bri, solidR, solidG, solidB); break;
    case SHOWDUINO_LAMP_FX_FADE_OUT: extraFadeOut(strip, bri, solidR, solidG, solidB); break;
    case SHOWDUINO_LAMP_FX_PULSE: extraPulse(strip, bri, solidR, solidG, solidB); break;
    case SHOWDUINO_LAMP_FX_BREATHE: extraBreathe(strip, bri, solidR, solidG, solidB); break;
    case SHOWDUINO_LAMP_FX_CANDLE: extraCandle(strip, bri); break;
    case SHOWDUINO_LAMP_FX_FIRE: extraFire(strip, bri); break;
    case SHOWDUINO_LAMP_FX_RANDOM_FLICKER: extraRandomFlicker(strip, bri, nowMs); break;
    case SHOWDUINO_LAMP_FX_FAULT_FLICKER: extraFault(strip, bri, nowMs); break;
    default: strip.clear(); break;
  }
  if (sNeedShow) strip.show();
}
