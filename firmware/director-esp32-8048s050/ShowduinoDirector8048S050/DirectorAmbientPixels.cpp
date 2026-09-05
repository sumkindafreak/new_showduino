#include "DirectorAmbientPixels.h"
#include "ShowduinoOsPalette.h"

#if SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_ENABLED

#include <Adafruit_NeoPixel.h>

/* Match Showduino OS theme (ShowduinoOsPalette). */
static const uint32_t kColAccent   = ShowduinoPalette::Accent;
static const uint32_t kColAccentLo = ShowduinoPalette::AccentDark;
static const uint32_t kColWarn     = ShowduinoPalette::Warn;
static const uint32_t kColOk       = ShowduinoPalette::Success;
static const uint32_t kColFault    = ShowduinoPalette::Danger;
static const uint32_t kColDimCyan  = ShowduinoPalette::AccentDim;

static Adafruit_NeoPixel *sStrip = nullptr;
static bool sReady = false;
static DirectorAmbientMode sMode = DIRECTOR_AMBIENT_BOOT;
static DirectorAmbientMode sPrevLogged = (DirectorAmbientMode)0xFF;
static uint8_t sBrightness = SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_BRIGHTNESS;
static uint32_t sLastFrameMs = 0;
static uint16_t sPhase = 0;
static uint32_t sSuccessUntilMs = 0;
static ShowState sLastShowState = SHOW_STATE_BOOTING;

static uint8_t scale8(uint8_t v, uint8_t scale) {
  return (uint8_t)((uint16_t)v * scale / 255);
}

static uint32_t packRgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void unpackRgb(uint32_t rgb, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = (uint8_t)((rgb >> 16) & 0xFF);
  *g = (uint8_t)((rgb >> 8) & 0xFF);
  *b = (uint8_t)(rgb & 0xFF);
}

static uint32_t fadeRgb(uint32_t rgb, uint8_t level) {
  uint8_t r, g, b;
  unpackRgb(rgb, &r, &g, &b);
  return packRgb(scale8(r, level), scale8(g, level), scale8(b, level));
}

static const char *modeName(DirectorAmbientMode m) {
  switch (m) {
    case DIRECTOR_AMBIENT_OFF:        return "OFF";
    case DIRECTOR_AMBIENT_BOOT:       return "BOOT";
    case DIRECTOR_AMBIENT_IDLE:       return "IDLE";
    case DIRECTOR_AMBIENT_READY:      return "READY";
    case DIRECTOR_AMBIENT_RUNNING:    return "RUNNING";
    case DIRECTOR_AMBIENT_WARNING:    return "WARNING";
    case DIRECTOR_AMBIENT_EMERGENCY:  return "EMERGENCY";
    case DIRECTOR_AMBIENT_FAULT:      return "FAULT";
    case DIRECTOR_AMBIENT_SUCCESS:    return "SUCCESS";
    default:                          return "?";
  }
}

static void setMode(DirectorAmbientMode m) {
  if (sMode == m) return;
  sMode = m;
  sPhase = 0;
  if (sPrevLogged != m) {
    Serial.printf("[Ambient] mode → %s\n", modeName(m));
    sPrevLogged = m;
  }
}

static void fillSolid(uint32_t rgb, uint8_t level) {
  if (!sStrip) return;
  const uint32_t c = fadeRgb(rgb, level);
  for (uint16_t i = 0; i < sStrip->numPixels(); i++) {
    sStrip->setPixelColor(i, c);
  }
}

static void renderFrame(uint32_t nowMs) {
  if (!sStrip) return;
  const uint16_t n = sStrip->numPixels();
  if (n == 0) return;

  switch (sMode) {
    case DIRECTOR_AMBIENT_OFF:
      sStrip->clear();
      break;

    case DIRECTOR_AMBIENT_BOOT: {
      /* Soft travelling cyan comet */
      sStrip->clear();
      const uint16_t head = sPhase % n;
      for (uint16_t t = 0; t < 4 && t < n; t++) {
        const uint16_t idx = (head + n - t) % n;
        const uint8_t lvl = (uint8_t)(180 - t * 45);
        sStrip->setPixelColor(idx, fadeRgb(kColAccent, lvl));
      }
      break;
    }

    case DIRECTOR_AMBIENT_IDLE:
      fillSolid(kColDimCyan, 40);
      break;

    case DIRECTOR_AMBIENT_READY:
      fillSolid(kColAccent, 140);
      break;

    case DIRECTOR_AMBIENT_RUNNING: {
      /* Gentle breathing + slow chase on accent */
      const uint8_t breath = (uint8_t)(90 + ((sPhase / 2) % 80));
      fillSolid(kColAccentLo, breath);
      const uint16_t head = sPhase % n;
      sStrip->setPixelColor(head, fadeRgb(kColAccent, 220));
      sStrip->setPixelColor((head + 1) % n, fadeRgb(kColAccent, 120));
      break;
    }

    case DIRECTOR_AMBIENT_WARNING: {
      const uint8_t pulse = (uint8_t)(70 + ((sPhase % 40) < 20 ? 100 : 0));
      fillSolid(kColWarn, pulse);
      break;
    }

    case DIRECTOR_AMBIENT_EMERGENCY:
    case DIRECTOR_AMBIENT_FAULT: {
      const uint8_t pulse = (uint8_t)(((sPhase % 20) < 10) ? 220 : 60);
      fillSolid(kColFault, pulse);
      break;
    }

    case DIRECTOR_AMBIENT_SUCCESS: {
      if (nowMs >= sSuccessUntilMs) {
        setMode(DIRECTOR_AMBIENT_READY);
        fillSolid(kColAccent, 140);
      } else {
        fillSolid(kColOk, 180);
      }
      break;
    }
  }

  sStrip->show();
  sPhase++;
}

void directorAmbientBegin() {
  if (sReady && sStrip) return;

  Serial.printf("[Ambient] NeoPixel pin=%d count=%u brightness=%u (GPIO18 reserved for GT911 INT)\n",
                SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_PIN,
                (unsigned)SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_COUNT,
                (unsigned)sBrightness);

  sStrip = new Adafruit_NeoPixel(SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_COUNT,
                                 SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_PIN,
                                 NEO_GRB + NEO_KHZ800);
  if (!sStrip) {
    Serial.println("[Ambient] alloc failed — ambient disabled");
    return;
  }

  sStrip->begin();
  sStrip->setBrightness(sBrightness);
  sStrip->clear();
  sStrip->show();
  sReady = true;
  sMode = DIRECTOR_AMBIENT_BOOT;
  sLastFrameMs = millis();
  Serial.println("[Ambient] ready (BOOT travelling)");
}

void directorAmbientSync(uint8_t linkState,
                         ShowState showState,
                         bool emergencyLocked,
                         bool stageConnected) {
  if (!sReady) return;

  /* Edge: show finished → brief success */
  if (sLastShowState != SHOW_STATE_FINISHED && showState == SHOW_STATE_FINISHED) {
    sSuccessUntilMs = millis() + 2500UL;
    setMode(DIRECTOR_AMBIENT_SUCCESS);
    sLastShowState = showState;
    return;
  }
  sLastShowState = showState;

  if (sMode == DIRECTOR_AMBIENT_SUCCESS && millis() < sSuccessUntilMs) {
    return; /* hold success colour */
  }

  if (emergencyLocked || showState == SHOW_STATE_EMERGENCY_STOP) {
    setMode(DIRECTOR_AMBIENT_EMERGENCY);
    return;
  }
  if (showState == SHOW_STATE_ERROR) {
    setMode(DIRECTOR_AMBIENT_FAULT);
    return;
  }
  if (showState == SHOW_STATE_BOOTING) {
    setMode(DIRECTOR_AMBIENT_BOOT);
    return;
  }
  if (linkState == LINK_DISCONNECTED || (!stageConnected && linkState != LINK_READY)) {
    setMode(DIRECTOR_AMBIENT_WARNING);
    return;
  }
  if (linkState == LINK_SEARCHING) {
    setMode(DIRECTOR_AMBIENT_BOOT);
    return;
  }
  if (showState == SHOW_STATE_RUNNING) {
    setMode(DIRECTOR_AMBIENT_RUNNING);
    return;
  }
  if (showState == SHOW_STATE_PAUSED) {
    setMode(DIRECTOR_AMBIENT_WARNING);
    return;
  }
  if (showState == SHOW_STATE_IDLE || showState == SHOW_STATE_SHOW_LOADED ||
      showState == SHOW_STATE_FINISHED) {
    setMode(linkState == LINK_READY ? DIRECTOR_AMBIENT_READY : DIRECTOR_AMBIENT_IDLE);
    return;
  }
  setMode(DIRECTOR_AMBIENT_IDLE);
}

void directorAmbientLoop(uint32_t nowMs) {
  if (!sReady || !sStrip) return;
  if ((nowMs - sLastFrameMs) < SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS) return;
  sLastFrameMs = nowMs;
  renderFrame(nowMs);
}

void directorAmbientSetBrightness(uint8_t brightness) {
  sBrightness = brightness;
  if (sStrip) {
    sStrip->setBrightness(sBrightness);
    Serial.printf("[Ambient] brightness=%u\n", (unsigned)sBrightness);
  }
}

uint8_t directorAmbientBrightness() { return sBrightness; }
DirectorAmbientMode directorAmbientMode() { return sMode; }
bool directorAmbientReady() { return sReady; }

#else

void directorAmbientBegin() {}
void directorAmbientLoop(uint32_t) {}
void directorAmbientSync(uint8_t, ShowState, bool, bool) {}
void directorAmbientSetBrightness(uint8_t) {}
uint8_t directorAmbientBrightness() { return 0; }
DirectorAmbientMode directorAmbientMode() { return DIRECTOR_AMBIENT_OFF; }
bool directorAmbientReady() { return false; }

#endif
