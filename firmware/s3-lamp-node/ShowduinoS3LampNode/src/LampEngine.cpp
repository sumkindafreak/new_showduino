#include "LampEngine.h"
#include "LampAudio.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <string.h>

static Adafruit_NeoPixel *sStrip = nullptr;
static uint8_t sRgb[SHOWDUINO_LAMP_PIXEL_COUNT][3];
static bool sEmergency = false;
static uint8_t sBri = SHOWDUINO_LAMP_DEFAULT_BRIGHTNESS;
static uint8_t sR = 255, sG = 180, sB = 40;
static uint8_t sSpeed = 50;
static uint8_t sIntensity = 50;
static ShowduinoLampFx sFx = SHOWDUINO_LAMP_FX_STEADY_FLAME;
static bool sCompat = false;
static ShowduinoCarbideMachine sMach;
static ShowduinoCarbideConfig sCfg;
static ShowduinoLampSound sLastSound = SHOWDUINO_LAMP_SND_NONE;
static uint32_t sLastTick = 0;
static uint32_t sNoise[SHOWDUINO_LAMP_PIXEL_COUNT];

static uint8_t scale(uint8_t v, uint8_t bri) {
  return (uint8_t)((uint16_t)v * (uint16_t)bri / 100u);
}

static uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static void putScratch(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= SHOWDUINO_LAMP_PIXEL_COUNT) return;
  sRgb[i][0] = scale(r, sBri);
  sRgb[i][1] = scale(g, sBri);
  sRgb[i][2] = scale(b, sBri);
}

static void showScratch() {
  if (!sStrip) return;
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sStrip->setPixelColor(i, sStrip->Color(sRgb[i][0], sRgb[i][1], sRgb[i][2]));
  }
  sStrip->show();
}

static void clearScratch() {
  memset(sRgb, 0, sizeof(sRgb));
  if (sStrip) {
    sStrip->clear();
    sStrip->show();
  }
}

static void syncAudio() {
  const ShowduinoLampSound want =
      showduino_lamp_effective_sound(sMach.sound, sEmergency ? 1 : 0);
  if (want == sLastSound) return;
  sLastSound = want;
  lampAudioPlay(want);
}

static void renderFlame(uint32_t now) {
  const ShowduinoCarbideState st = sMach.state;
  float body = 0.0f, flicker = 0.0f, hot = 0.0f;
  uint8_t baseR = 255, baseG = 120, baseB = 20;
  switch (st) {
    case SHOWDUINO_CARBIDE_STRIKING:
      body = 0.15f;
      flicker = 1.0f;
      hot = 0.9f;
      baseR = 255;
      baseG = 230;
      baseB = 180;
      break;
    case SHOWDUINO_CARBIDE_IGNITING:
      body = 0.55f;
      flicker = 0.55f;
      hot = 0.7f;
      break;
    case SHOWDUINO_CARBIDE_BURNING:
      body = 0.82f;
      flicker = 0.18f;
      hot = 0.55f;
      break;
    case SHOWDUINO_CARBIDE_LOW_FLAME:
      body = 0.42f;
      flicker = 0.28f;
      hot = 0.35f;
      baseG = 90;
      break;
    case SHOWDUINO_CARBIDE_UNSTABLE:
      body = 0.62f;
      flicker = 0.72f;
      hot = 0.5f;
      break;
    case SHOWDUINO_CARBIDE_FLARE:
      body = 1.0f;
      flicker = 0.35f;
      hot = 0.85f;
      baseG = 170;
      break;
    case SHOWDUINO_CARBIDE_EXTINGUISHING:
      body = 0.22f;
      flicker = 0.4f;
      hot = 0.2f;
      baseG = 70;
      break;
    default:
      clearScratch();
      return;
  }

  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sNoise[i] = hash32(sNoise[i] + now + (uint32_t)i * 17u + 1u);
    const float n = (float)(sNoise[i] & 255u) / 255.0f;
    const float n2 = (float)((sNoise[i] >> 8) & 255u) / 255.0f;
    const int ring = (i == 0) ? 0 : 1;
    const float center = (i == 0) ? 1.0f : 0.62f;
    const float wobble = 0.72f + 0.28f * sinf((float)now * 0.0045f + (float)i * 0.9f);
    float level = body * center * wobble;
    level += flicker * (n - 0.5f) * 0.55f;
    if (ring) level *= 0.78f + 0.22f * n2;
    if (level < 0.02f) level = 0.02f;
    if (level > 1.0f) level = 1.0f;
    uint8_t r = (uint8_t)(baseR * level);
    uint8_t g = (uint8_t)(baseG * level * (0.75f + hot * 0.25f * (i == 0 ? 1.0f : 0.55f)));
    uint8_t b = (uint8_t)(baseB * level * (0.35f + n2 * 0.2f));
    if (st == SHOWDUINO_CARBIDE_STRIKING && n > 0.82f) {
      r = 255;
      g = 255;
      b = 220;
    }
    putScratch(i, r, g, b);
  }
  showScratch();
}

static void renderCompat(uint32_t now) {
  const float ph = (float)(now / (20u + (uint32_t)(100 - sSpeed))) * 0.15f;
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    float k = 0.55f + 0.45f * sinf(ph + (float)i * 0.7f);
    if (sFx == SHOWDUINO_LAMP_FX_SOLID) k = 1.0f;
    putScratch(i, (uint8_t)(sR * k), (uint8_t)(sG * k), (uint8_t)(sB * k));
  }
  showScratch();
}

static void renderEmergency() {
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sRgb[i][0] = 255;
    sRgb[i][1] = 255;
    sRgb[i][2] = 255;
  }
  showScratch();
}

void lampEngineBegin() {
  sCfg = showduino_carbide_config_defaults();
  showduino_carbide_reset(&sMach, millis());
  sBri = lampConfigBrightness();
  sEmergency = false;
  sCompat = false;
  sLastSound = SHOWDUINO_LAMP_SND_NONE;
  memset(sRgb, 0, sizeof(sRgb));
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sNoise[i] = 0xA5A5u + (uint32_t)i * 97u;
  }
  if (SHOWDUINO_LAMP_PIXEL_PIN >= 0) {
    pinMode(SHOWDUINO_LAMP_PIXEL_PIN, OUTPUT);
    digitalWrite(SHOWDUINO_LAMP_PIXEL_PIN, LOW);
    sStrip = new Adafruit_NeoPixel(SHOWDUINO_LAMP_PIXEL_COUNT,
                                   SHOWDUINO_LAMP_PIXEL_PIN,
                                   NEO_GRB + NEO_KHZ800);
    sStrip->begin();
    sStrip->clear();
    sStrip->show();
    Serial.printf("[LAMP] Jewel x%u GPIO%d\n",
                  (unsigned)SHOWDUINO_LAMP_PIXEL_COUNT, SHOWDUINO_LAMP_PIXEL_PIN);
  } else {
    Serial.println("[LAMP] Jewel DATA pin UNCONFIRMED — renderer is visual-stub");
  }
}

void lampEngineService() {
  const uint32_t now = millis();
  sMach.nowMs = now;
  if (!sEmergency && !sCompat) {
    showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_TICK, &sCfg);
  }
  if (sEmergency || !sCompat) {
    syncAudio();
  }
  if (now - sLastTick < 20) return;
  sLastTick = now;
  if (sEmergency) {
    renderEmergency();
    return;
  }
  if (sCompat) {
    renderCompat(now);
    return;
  }
  renderFlame(now);
}

void lampEngineApplyEvent(ShowduinoCarbideEvent ev) {
  if (sEmergency && ev != SHOWDUINO_CARBIDE_EV_FORCE_OFF) return;
  sCompat = false;
  sMach.nowMs = millis();
  showduino_carbide_apply(&sMach, ev, &sCfg);
  switch (sMach.state) {
    case SHOWDUINO_CARBIDE_BURNING:
      sFx = SHOWDUINO_LAMP_FX_STEADY_FLAME;
      break;
    case SHOWDUINO_CARBIDE_LOW_FLAME:
      sFx = SHOWDUINO_LAMP_FX_LOW_FLAME;
      break;
    case SHOWDUINO_CARBIDE_UNSTABLE:
      sFx = SHOWDUINO_LAMP_FX_UNSTABLE;
      break;
    case SHOWDUINO_CARBIDE_FLARE:
      sFx = SHOWDUINO_LAMP_FX_FLARE;
      break;
    case SHOWDUINO_CARBIDE_EXTINGUISHING:
      sFx = SHOWDUINO_LAMP_FX_DYING_FLAME;
      break;
    default:
      sFx = SHOWDUINO_LAMP_FX_STEADY_FLAME;
      break;
  }
  syncAudio();
}

void lampEngineSetBrightness(uint8_t bri0to100) {
  sBri = showduino_lamp_clamp_bri(bri0to100);
  lampConfigSetBrightness(sBri);
}

uint8_t lampEngineBrightness() { return sBri; }

void lampEngineSetSolid(uint8_t r, uint8_t g, uint8_t b) {
  if (sEmergency) return;
  sR = r;
  sG = g;
  sB = b;
  sFx = SHOWDUINO_LAMP_FX_SOLID;
  sCompat = true;
  showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &sCfg);
  lampAudioStop();
  sLastSound = SHOWDUINO_LAMP_SND_NONE;
}

void lampEngineSetCompatFx(ShowduinoLampFx fx, uint8_t speed, uint8_t intensity) {
  if (sEmergency) return;
  const ShowduinoCarbideEvent ev = showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_FX, fx);
  if (ev != SHOWDUINO_CARBIDE_EV_NONE && ev != SHOWDUINO_CARBIDE_EV_STEADY) {
    lampEngineApplyEvent(ev);
    return;
  }
  if (ev == SHOWDUINO_CARBIDE_EV_STEADY) {
    if (!showduino_carbide_is_flame(sMach.state) &&
        sMach.state != SHOWDUINO_CARBIDE_STRIKING &&
        sMach.state != SHOWDUINO_CARBIDE_IGNITING) {
      lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_IGNITE);
    } else {
      lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_STEADY);
    }
    return;
  }
  sFx = fx;
  if (speed >= 1 && speed <= 100) sSpeed = speed;
  if (intensity <= 100) sIntensity = intensity;
  sCompat = true;
  showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &sCfg);
  lampAudioStop();
  sLastSound = SHOWDUINO_LAMP_SND_NONE;
}

ShowduinoLampFx lampEngineFx() { return sFx; }

const char *lampEngineFxToken() {
  if (sEmergency) return "-";
  if (!sCompat && sMach.state != SHOWDUINO_CARBIDE_OFF) {
    return showduino_carbide_state_name(sMach.state);
  }
  if (sCompat) {
    const ShowduinoLampFxInfo *info = showduino_lamp_fx_info(sFx);
    return info ? info->token : "UNKNOWN";
  }
  return "-";
}

const char *lampEngineFxDisplay() {
  if (!sCompat && sMach.state != SHOWDUINO_CARBIDE_OFF) {
    return showduino_carbide_state_name(sMach.state);
  }
  const ShowduinoLampFxInfo *info = showduino_lamp_fx_info(sFx);
  return info ? info->display : "Unknown";
}

ShowduinoCarbideState lampEngineCarbide() { return sMach.state; }
const char *lampEngineCarbideName() { return showduino_carbide_state_name(sMach.state); }

bool lampEngineActive() {
  return !sEmergency && (sCompat || showduino_carbide_is_lit(sMach.state));
}

bool lampEngineFlameLit() {
  return !sEmergency && showduino_carbide_is_lit(sMach.state);
}

void lampEngineOnEmergency(bool active) {
  sEmergency = active;
  if (active) {
    sCompat = false;
    showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &sCfg);
    sLastSound = SHOWDUINO_LAMP_SND_NONE;
    lampAudioPlay(SHOWDUINO_LAMP_SND_EMERGENCY);
    sLastSound = SHOWDUINO_LAMP_SND_EMERGENCY;
    renderEmergency();
  } else {
    lampAudioStop();
    sLastSound = SHOWDUINO_LAMP_SND_NONE;
    clearScratch();
  }
}

bool lampEngineEmergency() { return sEmergency; }

void lampEngineFill(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) putScratch(i, r, g, b);
  showScratch();
}

uint16_t lampEngineCount() { return SHOWDUINO_LAMP_PIXEL_COUNT; }
int lampEnginePin() { return SHOWDUINO_LAMP_PIXEL_PIN; }
bool lampEngineJewelReady() { return sStrip != nullptr; }

void lampEnginePixelRgb(uint8_t i, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (i >= SHOWDUINO_LAMP_PIXEL_COUNT) return;
  if (r) *r = sRgb[i][0];
  if (g) *g = sRgb[i][1];
  if (b) *b = sRgb[i][2];
}
