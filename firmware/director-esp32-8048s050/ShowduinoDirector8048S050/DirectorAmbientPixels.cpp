#include "DirectorAmbientPixels.h"
#include "DirectorUiMotion.h"
#include "ShowduinoOsPalette.h"

#if SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_ENABLED

#include <Adafruit_NeoPixel.h>

static const uint32_t kColAccent   = ShowduinoPalette::Accent;
static const uint32_t kColAccentLo = ShowduinoPalette::AccentDark;
static const uint32_t kColWarn     = ShowduinoPalette::Warn;
static const uint32_t kColOk       = ShowduinoPalette::Success;
static const uint32_t kColFault    = ShowduinoPalette::Danger;
static const uint32_t kColDimCyan  = ShowduinoPalette::AccentDim;

static Adafruit_NeoPixel *sStrip = nullptr;
static bool sReady = false;
static bool sUserEnabled = true;
static bool sHoldPresentation = true;
static DirectorAmbientMode sMode = DIRECTOR_AMBIENT_BOOT;
static DirectorAmbientMode sPersistent = DIRECTOR_AMBIENT_BOOT;
static DirectorAmbientMode sPrevLogged = (DirectorAmbientMode)0xFF;
static uint8_t sBrightness = SHOWDUINO_DIRECTOR_AMBIENT_PIXEL_BRIGHTNESS;
static uint32_t sLastFrameMs = 0;
static uint16_t sPhase = 0;
static uint32_t sPulseUntilMs = 0;
static ShowState sLastShowState = SHOW_STATE_BOOTING;
static bool sLocatorActive = false;
static uint32_t sLocatorUntilMs = 0;
static uint32_t sLocatorStartMs = 0;
static uint8_t sCurR = 0;
static uint8_t sCurG = 0;
static uint8_t sCurB = 0;
static uint8_t sTgtR = 0;
static uint8_t sTgtG = 0;
static uint8_t sTgtB = 0;
static uint32_t sFadeMs = DIRECTOR_AMBIENT_BOOT_FADE_MS;
static uint32_t sBootStartMs = 0;

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

static uint8_t toward8(uint8_t cur, uint8_t tgt, uint8_t step) {
  if (cur == tgt) return tgt;
  if (cur < tgt) {
    uint16_t n = (uint16_t)cur + step;
    return (n >= tgt) ? tgt : (uint8_t)n;
  }
  return (cur - step <= tgt) ? tgt : (uint8_t)(cur - step);
}

static const char *modeName(DirectorAmbientMode m) {
  switch (m) {
    case DIRECTOR_AMBIENT_OFF:        return "OFF";
    case DIRECTOR_AMBIENT_BOOT:       return "BOOT";
    case DIRECTOR_AMBIENT_IDLE:       return "IDLE";
    case DIRECTOR_AMBIENT_READY:      return "READY";
    case DIRECTOR_AMBIENT_RUNNING:    return "RUNNING";
    case DIRECTOR_AMBIENT_PAUSED:     return "PAUSED";
    case DIRECTOR_AMBIENT_STOPPED:    return "STOPPED";
    case DIRECTOR_AMBIENT_DEPLOYING:  return "DEPLOYING";
    case DIRECTOR_AMBIENT_WARNING:    return "WARNING";
    case DIRECTOR_AMBIENT_EMERGENCY:  return "EMERGENCY";
    case DIRECTOR_AMBIENT_FAULT:      return "FAULT";
    case DIRECTOR_AMBIENT_SUCCESS:    return "SUCCESS";
    case DIRECTOR_AMBIENT_DISCOVERY:  return "CONNECTING";
    case DIRECTOR_AMBIENT_DEGRADED:   return "DEGRADED";
    case DIRECTOR_AMBIENT_OFFLINE:    return "CONNECTION_LOST";
    default:                          return "?";
  }
}

static bool isEmergencyMode(DirectorAmbientMode m) {
  return m == DIRECTOR_AMBIENT_EMERGENCY;
}

static bool cosmeticsAllowed() {
  if (isEmergencyMode(sMode) || sMode == DIRECTOR_AMBIENT_FAULT ||
      sMode == DIRECTOR_AMBIENT_OFFLINE) {
    return true;
  }
  return sUserEnabled;
}

static void targetsForMode(DirectorAmbientMode m, uint8_t *r, uint8_t *g, uint8_t *b,
                           uint8_t *level) {
  uint32_t rgb = 0;
  uint8_t lvl = 0;
  switch (m) {
    case DIRECTOR_AMBIENT_OFF:
      rgb = 0; lvl = 0; break;
    case DIRECTOR_AMBIENT_BOOT:
      rgb = kColAccent; lvl = 110; break;
    case DIRECTOR_AMBIENT_IDLE:
      rgb = kColDimCyan; lvl = 40; break;
    case DIRECTOR_AMBIENT_READY:
      rgb = kColAccent; lvl = 140; break;
    case DIRECTOR_AMBIENT_DISCOVERY:
      rgb = kColAccent; lvl = 90; break;
    case DIRECTOR_AMBIENT_DEGRADED:
      rgb = kColWarn; lvl = 160; break;
    case DIRECTOR_AMBIENT_OFFLINE:
      rgb = kColFault; lvl = 70; break;
    case DIRECTOR_AMBIENT_RUNNING:
      rgb = kColAccent; lvl = 200; break;
    case DIRECTOR_AMBIENT_PAUSED:
      rgb = kColWarn; lvl = 90; break;
    case DIRECTOR_AMBIENT_STOPPED:
      rgb = kColAccentLo; lvl = 80; break;
    case DIRECTOR_AMBIENT_DEPLOYING:
      rgb = kColAccentLo; lvl = 120; break;
    case DIRECTOR_AMBIENT_WARNING:
      rgb = kColWarn; lvl = 150; break;
    case DIRECTOR_AMBIENT_EMERGENCY:
    case DIRECTOR_AMBIENT_FAULT:
      rgb = kColFault; lvl = 220; break;
    case DIRECTOR_AMBIENT_SUCCESS:
      rgb = kColOk; lvl = 180; break;
    default:
      rgb = kColDimCyan; lvl = 40; break;
  }
  uint8_t rr, gg, bb;
  unpackRgb(rgb, &rr, &gg, &bb);
  *r = scale8(rr, lvl);
  *g = scale8(gg, lvl);
  *b = scale8(bb, lvl);
  *level = lvl;
}

static void setMode(DirectorAmbientMode m, bool persistent, uint32_t fadeMs) {
  if (sMode == m && persistent) {
    sPersistent = m;
    return;
  }
  sMode = m;
  if (persistent) sPersistent = m;
  sPhase = 0;
  sFadeMs = fadeMs ? fadeMs : DIRECTOR_AMBIENT_FADE_MS;
  uint8_t lvl = 0;
  targetsForMode(m, &sTgtR, &sTgtG, &sTgtB, &lvl);
  (void)lvl;
  if (isEmergencyMode(m) || m == DIRECTOR_AMBIENT_FAULT) {
    unpackRgb(kColFault, &sCurR, &sCurG, &sCurB);
    sCurR = scale8(sCurR, 220);
    sCurG = 0;
    sCurB = 0;
    sTgtR = sCurR;
    sTgtG = sCurG;
    sTgtB = sCurB;
  }
  if (sPrevLogged != m) {
    Serial.printf("[Ambient] mode -> %s\n", modeName(m));
    sPrevLogged = m;
  }
}

static void restorePersistent() {
  setMode(sPersistent, true, DIRECTOR_AMBIENT_FADE_MS);
}

static uint32_t currentPacked() {
  return packRgb(sCurR, sCurG, sCurB);
}

static void showSolid(uint32_t rgb) {
  if (!sStrip) return;
  uint8_t r, g, b;
  unpackRgb(rgb, &r, &g, &b);
  r = scale8(r, sBrightness);
  g = scale8(g, sBrightness);
  b = scale8(b, sBrightness);
  const uint32_t c = packRgb(r, g, b);
  for (uint16_t i = 0; i < sStrip->numPixels(); i++) {
    sStrip->setPixelColor(i, c);
  }
  sStrip->show();
}

static void renderFrame(uint32_t nowMs) {
  if (!sStrip) return;
  const uint16_t n = sStrip->numPixels();
  if (n == 0) return;

  if (!cosmeticsAllowed()) {
    sStrip->clear();
    sStrip->show();
    return;
  }

  uint8_t step = 24;
  if (sFadeMs >= SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS) {
    step = (uint8_t)((255UL * SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS) / sFadeMs);
    if (step < 4) step = 4;
  }

  if (sMode == DIRECTOR_AMBIENT_BOOT && sBootStartMs != 0) {
    const uint32_t elapsed = nowMs - sBootStartMs;
    if (elapsed < DIRECTOR_AMBIENT_BOOT_FADE_MS) {
      step = (uint8_t)((255UL * SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS) / DIRECTOR_AMBIENT_BOOT_FADE_MS);
      if (step < 2) step = 2;
    }
  }

  sCurR = toward8(sCurR, sTgtR, step);
  sCurG = toward8(sCurG, sTgtG, step);
  sCurB = toward8(sCurB, sTgtB, step);

  if (sMode == DIRECTOR_AMBIENT_SUCCESS || sMode == DIRECTOR_AMBIENT_DEPLOYING) {
    if (sPulseUntilMs != 0 && nowMs >= sPulseUntilMs) {
      sPulseUntilMs = 0;
      restorePersistent();
    }
  }

  if (sMode == DIRECTOR_AMBIENT_DISCOVERY) {
    const uint8_t pulse = (uint8_t)(70 + ((sPhase / 3) % 70));
    uint8_t r, g, b, lvl;
    targetsForMode(sMode, &r, &g, &b, &lvl);
    (void)lvl;
    sTgtR = scale8(r, pulse);
    sTgtG = scale8(g, pulse);
    sTgtB = scale8(b, pulse);
  } else if (sMode == DIRECTOR_AMBIENT_RUNNING) {
    const uint8_t breath = (uint8_t)(160 + ((sPhase / 2) % 70));
    uint8_t r, g, b, lvl;
    targetsForMode(sMode, &r, &g, &b, &lvl);
    (void)lvl;
    sTgtR = scale8(r, breath);
    sTgtG = scale8(g, breath);
    sTgtB = scale8(b, breath);
  } else if (sMode == DIRECTOR_AMBIENT_WARNING || sMode == DIRECTOR_AMBIENT_PAUSED) {
    const uint8_t pulse = (uint8_t)(80 + ((sPhase % 40) < 20 ? 70 : 0));
    uint8_t r, g, b, lvl;
    targetsForMode(sMode, &r, &g, &b, &lvl);
    (void)lvl;
    sTgtR = scale8(r, pulse);
    sTgtG = scale8(g, pulse);
    sTgtB = scale8(b, pulse);
  } else if (sMode == DIRECTOR_AMBIENT_EMERGENCY || sMode == DIRECTOR_AMBIENT_FAULT) {
    const uint8_t pulse = (uint8_t)(((sPhase % 20) < 10) ? 220 : 70);
    sCurR = pulse;
    sCurG = 0;
    sCurB = 0;
    sTgtR = sCurR;
    sTgtG = 0;
    sTgtB = 0;
  } else if (sMode == DIRECTOR_AMBIENT_OFFLINE) {
    const uint8_t pulse = (uint8_t)(40 + ((sPhase % 30) < 15 ? 50 : 0));
    sTgtR = pulse;
    sTgtG = 8;
    sTgtB = 8;
  }

  showSolid(currentPacked());
  sPhase++;
}

static void renderLocator(uint32_t nowMs) {
  if (!sStrip) return;
  static const uint16_t kPhaseMs[] = {80, 80, 80, 200, 80, 80, 80, 200};
  uint32_t elapsed = nowMs - sLocatorStartMs;
  uint32_t cycle = 0;
  for (uint8_t i = 0; i < 8; i++) cycle += kPhaseMs[i];
  if (cycle == 0) cycle = 1;
  elapsed %= cycle;

  uint8_t phase = 0;
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 8; i++) {
    acc += kPhaseMs[i];
    if (elapsed < acc) {
      phase = i;
      break;
    }
  }

  sStrip->clear();
  const bool redOn = (phase == 0 || phase == 2);
  const bool blueOn = (phase == 4 || phase == 6);
  if (sStrip->numPixels() > 0 && redOn) {
    sStrip->setPixelColor(0, packRgb(255, 0, 0));
  }
  if (sStrip->numPixels() > 1 && blueOn) {
    sStrip->setPixelColor(1, packRgb(0, 0, 255));
  }
  sStrip->show();
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
    Serial.println("[Ambient] alloc failed - ambient disabled");
    return;
  }

  sStrip->begin();
  sStrip->setBrightness(255);
  sStrip->clear();
  sStrip->show();
  sReady = true;
  sHoldPresentation = true;
  sCurR = sCurG = sCurB = 0;
  sBootStartMs = millis();
  setMode(DIRECTOR_AMBIENT_BOOT, true, DIRECTOR_AMBIENT_BOOT_FADE_MS);
  sLastFrameMs = millis();
  Serial.println("[Ambient] ready (BOOT fade from dark)");
}

void directorAmbientSetState(DirectorAmbientMode mode) {
  if (!sReady) return;
  if (sHoldPresentation && !isEmergencyMode(mode) && mode != DIRECTOR_AMBIENT_FAULT) {
    return;
  }
  sPulseUntilMs = 0;
  setMode(mode, true, (mode == DIRECTOR_AMBIENT_BOOT)
                          ? DIRECTOR_AMBIENT_BOOT_FADE_MS
                          : DIRECTOR_AMBIENT_FADE_MS);
}

void directorAmbientPulse(DirectorAmbientEvent event) {
  if (!sReady) return;
  if (isEmergencyMode(sPersistent) || isEmergencyMode(sMode)) return;
  if (sHoldPresentation) return;

  uint32_t holdMs = 900;
  DirectorAmbientMode pulse = sPersistent;
  switch (event) {
    case DIRECTOR_AMBIENT_EVT_NODE_DISCOVERED:
      pulse = DIRECTOR_AMBIENT_SUCCESS;
      holdMs = 700;
      break;
    case DIRECTOR_AMBIENT_EVT_DEPLOY_REQUESTED:
    case DIRECTOR_AMBIENT_EVT_START_REQUESTED:
      pulse = DIRECTOR_AMBIENT_DEPLOYING;
      holdMs = 500;
      break;
    case DIRECTOR_AMBIENT_EVT_DEPLOY_SUCCESS:
      pulse = DIRECTOR_AMBIENT_SUCCESS;
      holdMs = 900;
      break;
    case DIRECTOR_AMBIENT_EVT_PAUSE_REQUESTED:
      pulse = DIRECTOR_AMBIENT_PAUSED;
      holdMs = 400;
      break;
    case DIRECTOR_AMBIENT_EVT_RESUME_REQUESTED:
      pulse = DIRECTOR_AMBIENT_DEPLOYING;
      holdMs = 400;
      break;
    case DIRECTOR_AMBIENT_EVT_STOP_REQUESTED:
      pulse = DIRECTOR_AMBIENT_STOPPED;
      holdMs = 400;
      break;
    case DIRECTOR_AMBIENT_EVT_CONNECTION_RESTORED:
      pulse = DIRECTOR_AMBIENT_SUCCESS;
      holdMs = 800;
      break;
    default:
      return;
  }
  sPulseUntilMs = millis() + holdMs;
  setMode(pulse, false, 180);
}

void directorAmbientHoldPresentation(bool hold) {
  sHoldPresentation = hold;
}

bool directorAmbientPresentationHeld() {
  return sHoldPresentation;
}

void directorAmbientSync(uint8_t linkState,
                         ShowState showState,
                         bool emergencyLocked,
                         bool stageConnected,
                         bool synchronising,
                         bool degraded) {
  if (!sReady) return;

  sLastShowState = showState;

  if (emergencyLocked || showState == SHOW_STATE_EMERGENCY_STOP) {
    sPulseUntilMs = 0;
    sHoldPresentation = false;
    setMode(DIRECTOR_AMBIENT_EMERGENCY, true, 0);
    return;
  }

  if (sHoldPresentation) {
    return;
  }

  if (sPulseUntilMs != 0 && millis() < sPulseUntilMs) {
    return;
  }

  if (showState == SHOW_STATE_ERROR) {
    setMode(DIRECTOR_AMBIENT_FAULT, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (linkState == LINK_DISCONNECTED) {
    setMode(DIRECTOR_AMBIENT_OFFLINE, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (linkState == LINK_SEARCHING || synchronising ||
      (!stageConnected && linkState != LINK_READY)) {
    setMode(DIRECTOR_AMBIENT_DISCOVERY, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (degraded) {
    setMode(DIRECTOR_AMBIENT_DEGRADED, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (showState == SHOW_STATE_RUNNING) {
    setMode(DIRECTOR_AMBIENT_RUNNING, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (showState == SHOW_STATE_PAUSED) {
    setMode(DIRECTOR_AMBIENT_PAUSED, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (showState == SHOW_STATE_FINISHED) {
    setMode(DIRECTOR_AMBIENT_STOPPED, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (showState == SHOW_STATE_IDLE || showState == SHOW_STATE_SHOW_LOADED) {
    setMode(linkState == LINK_READY ? DIRECTOR_AMBIENT_READY : DIRECTOR_AMBIENT_IDLE,
            true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  if (showState == SHOW_STATE_BOOTING) {
    setMode(DIRECTOR_AMBIENT_DISCOVERY, true, DIRECTOR_AMBIENT_FADE_MS);
    return;
  }
  setMode(DIRECTOR_AMBIENT_IDLE, true, DIRECTOR_AMBIENT_FADE_MS);
}

void directorAmbientStartLocator(uint32_t nowMs) {
  sLocatorActive = true;
  sLocatorStartMs = nowMs;
  sLocatorUntilMs = nowMs + SHOWDUINO_DIRECTOR_LOCATOR_DURATION_MS;
  Serial.println("[LOCATOR] NeoPixel locator active");
}

bool directorAmbientLocatorActive() {
  return sLocatorActive;
}

void directorAmbientLoop(uint32_t nowMs) {
  if (!sReady || !sStrip) return;
  if ((nowMs - sLastFrameMs) < SHOWDUINO_DIRECTOR_AMBIENT_FRAME_MS) return;
  sLastFrameMs = nowMs;
  if (sLocatorActive) {
    if ((int32_t)(nowMs - sLocatorUntilMs) >= 0) {
      sLocatorActive = false;
      Serial.println("[LOCATOR] NeoPixel locator finished");
      renderFrame(nowMs);
    } else {
      renderLocator(nowMs);
    }
    return;
  }
  renderFrame(nowMs);
}

void directorAmbientSetEnabled(bool enabled) {
  sUserEnabled = enabled;
  Serial.printf("[Ambient] leds=%s\n", enabled ? "ON" : "OFF");
}

bool directorAmbientEnabled() { return sUserEnabled; }

void directorAmbientSetBrightness(uint8_t brightness) {
  sBrightness = brightness ? brightness : 1;
  Serial.printf("[Ambient] brightness=%u\n", (unsigned)sBrightness);
}

uint8_t directorAmbientBrightness() { return sBrightness; }
DirectorAmbientMode directorAmbientMode() { return sMode; }
bool directorAmbientReady() { return sReady; }

#else

void directorAmbientBegin() {}
void directorAmbientLoop(uint32_t) {}
void directorAmbientSync(uint8_t, ShowState, bool, bool, bool, bool) {}
void directorAmbientSetState(DirectorAmbientMode) {}
void directorAmbientPulse(DirectorAmbientEvent) {}
void directorAmbientHoldPresentation(bool) {}
bool directorAmbientPresentationHeld() { return false; }
void directorAmbientSetEnabled(bool) {}
bool directorAmbientEnabled() { return false; }
void directorAmbientStartLocator(uint32_t) {}
bool directorAmbientLocatorActive() { return false; }
void directorAmbientSetBrightness(uint8_t) {}
uint8_t directorAmbientBrightness() { return 0; }
DirectorAmbientMode directorAmbientMode() { return DIRECTOR_AMBIENT_OFF; }
bool directorAmbientReady() { return false; }

#endif
