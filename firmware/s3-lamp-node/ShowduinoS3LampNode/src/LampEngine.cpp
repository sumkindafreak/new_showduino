#include "LampEngine.h"
#include "LampAudio.h"
#include "LampConfig.h"
#include "../BoardConfig.h"

#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <string.h>

static Adafruit_NeoPixel *sStrip = nullptr;
static uint8_t sRgb[SHOWDUINO_LAMP_PIXEL_COUNT][3];
static float sLevel[SHOWDUINO_LAMP_PIXEL_COUNT];
static float sTarget[SHOWDUINO_LAMP_PIXEL_COUNT];
static uint32_t sWalk[SHOWDUINO_LAMP_PIXEL_COUNT];
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
static uint8_t sBlowStress = 0;
static uint32_t sRecoverUntil = 0;
static int sIdentify = -1;
static uint32_t sIdentifyUntil = 0;
static ShowduinoCarbideVisual sVisual = SHOWDUINO_CARBIDE_VIS_BLACK;
static ShowduinoCarbideState sLastState = SHOWDUINO_CARBIDE_OFF;

static uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

static uint8_t scale(uint8_t v, uint8_t bri) {
  return (uint8_t)((uint16_t)v * (uint16_t)bri / 100u);
}

static uint8_t coreIndex() { return lampConfigJewelCore(); }

static void putRaw(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= SHOWDUINO_LAMP_PIXEL_COUNT) return;
  sRgb[i][0] = r;
  sRgb[i][1] = g;
  sRgb[i][2] = b;
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
  memset(sLevel, 0, sizeof(sLevel));
  memset(sTarget, 0, sizeof(sTarget));
  if (sStrip) {
    sStrip->clear();
    sStrip->show();
  }
}

static void resetFlameWalk(uint32_t now) {
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sLevel[i] = 0.0f;
    sTarget[i] = 0.0f;
    sWalk[i] = 0xA5A5u + (uint32_t)i * 97u + now;
  }
}

static void ensureStrip() {
  if (sStrip || SHOWDUINO_LAMP_PIXEL_PIN < 0) return;
  pinMode(SHOWDUINO_LAMP_PIXEL_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_LAMP_PIXEL_PIN, LOW);
  sStrip = new Adafruit_NeoPixel(SHOWDUINO_LAMP_PIXEL_COUNT,
                                 SHOWDUINO_LAMP_PIXEL_PIN,
                                 NEO_GRB + NEO_KHZ800);
  sStrip->begin();
  sStrip->clear();
  sStrip->show();
}

static void carbideRgb(float heat, float warm, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (heat < 0.0f) heat = 0.0f;
  if (heat > 1.0f) heat = 1.0f;
  if (warm < 0.0f) warm = 0.0f;
  if (warm > 1.0f) warm = 1.0f;
  const float blueR = 70.0f, blueG = 140.0f, blueB = 255.0f;
  const float whiteR = 220.0f, whiteG = 235.0f, whiteB = 255.0f;
  const float warmR = 255.0f, warmG = 228.0f, warmB = 150.0f;
  const float hr = blueR + (whiteR - blueR) * heat;
  const float hg = blueG + (whiteG - blueG) * heat;
  const float hb = blueB + (whiteB - blueB) * heat;
  *r = (uint8_t)(hr + (warmR - hr) * warm);
  *g = (uint8_t)(hg + (warmG - hg) * warm);
  *b = (uint8_t)(hb + (warmB - hb) * warm);
}

static void paintCarbide(uint8_t i, float level, float heat, float warm) {
  uint8_t r, g, b;
  if (level < 0.0f) level = 0.0f;
  if (level > 1.0f) level = 1.0f;
  carbideRgb(heat, warm, &r, &g, &b);
  putRaw(i, scale((uint8_t)(r * level), sBri),
         scale((uint8_t)(g * level), sBri),
         scale((uint8_t)(b * level), sBri));
}

static void applyTuneLocked() {
  sCfg = showduino_carbide_config_defaults();
  const uint8_t spd = lampConfigIgnitionSpeed();
  sCfg.igniteMs = (SHOWDUINO_CARBIDE_IGNITE_MS * 100u) / (spd ? spd : 100u);
  if (sCfg.igniteMs < 700) sCfg.igniteMs = 700;
  if (sCfg.igniteMs > 3200) sCfg.igniteMs = 3200;
}

static void syncAudio() {
  const ShowduinoLampSound want =
      showduino_lamp_effective_sound(sMach.sound, sEmergency ? 1 : 0);
  if (want == sLastSound) return;
  sLastSound = want;
  lampAudioPlay(want);
}

static void renderIdentify() {
  memset(sRgb, 0, sizeof(sRgb));
  if (sIdentify >= 0 && sIdentify < (int)SHOWDUINO_LAMP_PIXEL_COUNT) {
    putRaw((uint8_t)sIdentify, scale(255, sBri), scale(255, sBri), scale(40, sBri));
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

static void renderFlint(ShowduinoCarbideVisual vis) {
  const uint8_t core = coreIndex();
  const uint8_t near = showduino_jewel_ring_index(core, 0);
  memset(sRgb, 0, sizeof(sRgb));
  if (vis == SHOWDUINO_CARBIDE_VIS_FLINT_CORE) {
    putRaw(core, scale(255, sBri), scale(255, sBri), scale(255, sBri));
  } else if (vis == SHOWDUINO_CARBIDE_VIS_FLINT_NEAR) {
    putRaw(near, scale(180, sBri), scale(210, sBri), scale(255, sBri));
  }
  showScratch();
}

static void renderCatch(ShowduinoCarbideVisual vis, uint32_t now) {
  const uint8_t core = coreIndex();
  const float act = (float)lampConfigFlameActivity() / 100.0f;
  float coreL = 0.08f, nearL = 0.0f, outerL = 0.0f, heat = 0.15f, warm = 0.05f;
  switch (vis) {
    case SHOWDUINO_CARBIDE_VIS_CATCH_A:
      coreL = 0.12f;
      heat = 0.08f;
      warm = 0.0f;
      break;
    case SHOWDUINO_CARBIDE_VIS_CATCH_B:
      coreL = 0.28f;
      nearL = 0.14f;
      heat = 0.25f;
      warm = 0.12f;
      break;
    case SHOWDUINO_CARBIDE_VIS_CATCH_DIP:
      coreL = 0.10f;
      nearL = 0.04f;
      heat = 0.18f;
      warm = 0.04f;
      break;
    case SHOWDUINO_CARBIDE_VIS_CATCH_C:
      coreL = 0.48f;
      nearL = 0.28f;
      outerL = 0.16f;
      heat = 0.42f;
      warm = 0.22f;
      break;
    default:
      coreL = 0.72f;
      nearL = 0.50f;
      outerL = 0.34f;
      heat = 0.55f;
      warm = 0.32f;
      break;
  }
  const float flutter = 0.85f + 0.15f * sinf((float)now * 0.018f);
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    float l = outerL;
    if (i == core) l = coreL;
    else if (i == showduino_jewel_ring_index(core, 0) ||
             i == showduino_jewel_ring_index(core, 1)) {
      l = nearL;
    } else {
      const float irreg = (float)((hash32(now + (uint32_t)i * 19u) >> 8) & 255u) / 255.0f;
      if (vis == SHOWDUINO_CARBIDE_VIS_CATCH_C && irreg < 0.35f) l *= 0.25f;
    }
    l *= flutter * (0.75f + 0.25f * act);
    paintCarbide(i, l, heat, warm);
    sLevel[i] = l;
    sTarget[i] = l;
  }
  showScratch();
}

static void stepOrganic(uint32_t now, float body, float flicker, float stress) {
  const uint8_t core = coreIndex();
  const float act = (float)lampConfigFlameActivity() / 100.0f;
  const float flk = (float)lampConfigFlickerAmount() / 100.0f * flicker;
  const float drift = 0.92f + 0.08f * sinf((float)now * (0.0018f + 0.0012f * act));
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    sWalk[i] = hash32(sWalk[i] + 1u + (now >> 3) + (uint32_t)i * 13u);
    const float n = (float)(sWalk[i] & 255u) / 255.0f;
    const float n2 = (float)((sWalk[i] >> 8) & 255u) / 255.0f;
    const int isCore = (i == core);
    const int isNear = (!isCore &&
                        (i == showduino_jewel_ring_index(core, 0) ||
                         i == showduino_jewel_ring_index(core, 1)));
    float ring = isCore ? 1.0f : (isNear ? 0.78f : 0.58f);
    float t = body * ring * drift;
    t += flk * (n - 0.5f) * (isCore ? 0.12f : 0.38f);
    if (!isCore) t -= stress * (0.18f + 0.35f * n2);
    if (!isCore && stress > 0.55f && n2 > 0.72f) t = 0.0f;
    if (isCore) t *= (1.0f - stress * 0.35f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    sTarget[i] = t;
    const float k = isCore ? 0.18f : 0.11f;
    sLevel[i] += (sTarget[i] - sLevel[i]) * k;
  }
}

static void renderLiving(ShowduinoCarbideVisual vis, uint32_t now) {
  float body = 0.78f, flicker = 0.22f, stress = (float)sBlowStress / 100.0f;
  float heat = 0.55f, warm = 0.28f;
  const uint8_t core = coreIndex();
  if (sMach.state == SHOWDUINO_CARBIDE_LOW_FLAME) {
    body = 0.42f;
    flicker = 0.28f;
    heat = 0.40f;
    warm = 0.18f;
  } else if (sMach.state == SHOWDUINO_CARBIDE_FLARE) {
    body = 0.95f;
    flicker = 0.30f;
    heat = 0.75f;
    warm = 0.22f;
  }
  if (vis == SHOWDUINO_CARBIDE_VIS_PUFF) {
    body = 0.38f;
    flicker = 0.85f;
    stress = stress > 0.35f ? stress : 0.55f;
    heat = 0.22f;
    warm = 0.06f;
  } else if (vis == SHOWDUINO_CARBIDE_VIS_RECOVER) {
    body = 0.58f;
    flicker = 0.32f;
    stress *= 0.35f;
    heat = 0.40f;
    warm = 0.18f;
  }
  stepOrganic(now, body, flicker, stress);
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    const int isCore = (i == core);
    const float h = heat + (isCore ? 0.12f : -0.06f);
    const float w = warm + (isCore ? 0.04f : 0.10f);
    paintCarbide(i, sLevel[i], h, w);
  }
  showScratch();
}

static void renderExtinguish(ShowduinoCarbideVisual vis) {
  const uint8_t core = coreIndex();
  memset(sRgb, 0, sizeof(sRgb));
  if (vis == SHOWDUINO_CARBIDE_VIS_EXT_OUTER) {
    paintCarbide(core, 0.38f, 0.35f, 0.12f);
    paintCarbide(showduino_jewel_ring_index(core, 0), 0.12f, 0.20f, 0.04f);
    paintCarbide(showduino_jewel_ring_index(core, 1), 0.10f, 0.20f, 0.04f);
  } else if (vis == SHOWDUINO_CARBIDE_VIS_EXT_CORE) {
    paintCarbide(core, 0.22f, 0.18f, 0.02f);
  } else if (vis == SHOWDUINO_CARBIDE_VIS_EXT_REMNANT) {
    paintCarbide(core, 0.10f, 0.08f, 0.0f);
  } else if (vis == SHOWDUINO_CARBIDE_VIS_EXT_EMBER) {
    paintCarbide(core, 0.04f, 0.12f, 0.05f);
  }
  showScratch();
}

static void renderCompat(uint32_t now) {
  const float ph = (float)(now / (20u + (uint32_t)(100 - sSpeed))) * 0.15f;
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    float k = 0.55f + 0.45f * sinf(ph + (float)i * 0.7f);
    if (sFx == SHOWDUINO_LAMP_FX_SOLID) k = 1.0f;
    putRaw(i, scale((uint8_t)(sR * k), sBri),
           scale((uint8_t)(sG * k), sBri),
           scale((uint8_t)(sB * k), sBri));
  }
  showScratch();
}

void lampEngineBlackoutEarly() {
  memset(sRgb, 0, sizeof(sRgb));
  ensureStrip();
  if (sStrip) {
    sStrip->clear();
    sStrip->show();
  }
}

void lampEngineBegin() {
  applyTuneLocked();
  showduino_carbide_reset(&sMach, millis());
  sBri = lampConfigBrightness();
  sEmergency = false;
  sCompat = false;
  sBlowStress = 0;
  sRecoverUntil = 0;
  sIdentify = -1;
  sLastSound = SHOWDUINO_LAMP_SND_NONE;
  sLastState = SHOWDUINO_CARBIDE_OFF;
  sVisual = SHOWDUINO_CARBIDE_VIS_BLACK;
  resetFlameWalk(millis());
  ensureStrip();
  clearScratch();
  if (sStrip) {
    Serial.printf("[LAMP] Jewel x%u GPIO%d BLACKOUT\n",
                  (unsigned)SHOWDUINO_LAMP_PIXEL_COUNT, SHOWDUINO_LAMP_PIXEL_PIN);
  } else {
    Serial.println("[LAMP] Jewel DATA pin UNCONFIRMED — renderer is visual-stub");
  }
}

void lampEngineApplyTune() {
  applyTuneLocked();
  sBri = lampConfigBrightness();
}

void lampEngineService() {
  const uint32_t now = millis();
  sMach.nowMs = now;
  if (sIdentify >= 0 && (int32_t)(now - sIdentifyUntil) >= 0) {
    sIdentify = -1;
    clearScratch();
  }
  if (!sEmergency && !sCompat) {
    showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_TICK, &sCfg);
  }
  if (sMach.state != sLastState) {
    if (sMach.state == SHOWDUINO_CARBIDE_STRIKING ||
        sMach.state == SHOWDUINO_CARBIDE_OFF) {
      resetFlameWalk(now);
    }
    if (sLastState == SHOWDUINO_CARBIDE_UNSTABLE &&
        showduino_carbide_is_flame(sMach.state)) {
      sRecoverUntil = now + SHOWDUINO_CARBIDE_RECOVER_MS;
    }
    sLastState = sMach.state;
  }
  if (sEmergency || !sCompat) syncAudio();
  if (now - sLastTick < 25) return;
  sLastTick = now;

  const uint8_t recovering =
      (!sEmergency && sIdentify < 0 && sRecoverUntil && now < sRecoverUntil &&
       showduino_carbide_is_flame(sMach.state) &&
       sMach.state != SHOWDUINO_CARBIDE_UNSTABLE) ? 1 : 0;
  const uint8_t puff =
      (!sEmergency && (sMach.state == SHOWDUINO_CARBIDE_UNSTABLE || sBlowStress > 12))
          ? 1 : 0;
  sVisual = showduino_carbide_visual(
      sMach.state, showduino_carbide_elapsed_ms(&sMach), sCfg.igniteMs,
      sCfg.extinguishMs, puff, recovering, sEmergency ? 1 : 0, sIdentify);

  if (sEmergency) {
    renderEmergency();
    return;
  }
  if (sIdentify >= 0) {
    const uint32_t left = sIdentifyUntil - now;
    const uint32_t used = (SHOWDUINO_CARBIDE_IDENTIFY_DWELL_MS * 7u) - left;
    {
      const int next = (int)((used / SHOWDUINO_CARBIDE_IDENTIFY_DWELL_MS) % 7u);
      if (next != sIdentify) {
        sIdentify = next;
        Serial.printf("[LAMP] PIXEL IDENTIFY %d\n", sIdentify);
      }
    }
    renderIdentify();
    return;
  }
  if (sCompat) {
    renderCompat(now);
    return;
  }
  switch (sVisual) {
    case SHOWDUINO_CARBIDE_VIS_BLACK:
    case SHOWDUINO_CARBIDE_VIS_FLINT_DARK:
      clearScratch();
      break;
    case SHOWDUINO_CARBIDE_VIS_FLINT_CORE:
    case SHOWDUINO_CARBIDE_VIS_FLINT_NEAR:
      renderFlint(sVisual);
      break;
    case SHOWDUINO_CARBIDE_VIS_CATCH_A:
    case SHOWDUINO_CARBIDE_VIS_CATCH_B:
    case SHOWDUINO_CARBIDE_VIS_CATCH_DIP:
    case SHOWDUINO_CARBIDE_VIS_CATCH_C:
    case SHOWDUINO_CARBIDE_VIS_CATCH_D:
      renderCatch(sVisual, now);
      break;
    case SHOWDUINO_CARBIDE_VIS_EXT_OUTER:
    case SHOWDUINO_CARBIDE_VIS_EXT_CORE:
    case SHOWDUINO_CARBIDE_VIS_EXT_REMNANT:
    case SHOWDUINO_CARBIDE_VIS_EXT_EMBER:
      renderExtinguish(sVisual);
      break;
    default:
      renderLiving(sVisual, now);
      break;
  }
}

void lampEngineApplyEvent(ShowduinoCarbideEvent ev) {
  if (sEmergency && ev != SHOWDUINO_CARBIDE_EV_FORCE_OFF) return;
  sCompat = false;
  lampEngineStopIdentify();
  sMach.nowMs = millis();
  if (ev == SHOWDUINO_CARBIDE_EV_IGNITE) resetFlameWalk(sMach.nowMs);
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
      sBlowStress = 0;
      sRecoverUntil = 0;
      break;
    default:
      sFx = SHOWDUINO_LAMP_FX_STEADY_FLAME;
      break;
  }
  syncAudio();
}

void lampEngineSetBlowStress(uint8_t stress0to100) {
  sBlowStress = stress0to100 > 100 ? 100 : stress0to100;
}

uint8_t lampEngineBlowStress() { return sBlowStress; }
ShowduinoCarbideVisual lampEngineVisual() { return sVisual; }
const char *lampEngineVisualName() { return showduino_carbide_visual_name(sVisual); }

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
  lampEngineStopIdentify();
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
  lampEngineStopIdentify();
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
  sIdentify = -1;
  sBlowStress = 0;
  sRecoverUntil = 0;
  if (active) {
    sCompat = false;
    showduino_carbide_apply(&sMach, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &sCfg);
    sLastSound = SHOWDUINO_LAMP_SND_NONE;
    lampAudioPlay(SHOWDUINO_LAMP_SND_EMERGENCY);
    sLastSound = SHOWDUINO_LAMP_SND_EMERGENCY;
    sVisual = SHOWDUINO_CARBIDE_VIS_EMERGENCY;
    renderEmergency();
  } else {
    lampAudioStop();
    sLastSound = SHOWDUINO_LAMP_SND_NONE;
    sVisual = SHOWDUINO_CARBIDE_VIS_BLACK;
    clearScratch();
  }
}

bool lampEngineEmergency() { return sEmergency; }

void lampEngineFill(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < SHOWDUINO_LAMP_PIXEL_COUNT; i++) {
    putRaw(i, scale(r, sBri), scale(g, sBri), scale(b, sBri));
  }
  showScratch();
}

bool lampEngineStartIdentify() {
  if (sEmergency) return false;
  sIdentify = 0;
  sIdentifyUntil = millis() + (SHOWDUINO_CARBIDE_IDENTIFY_DWELL_MS * 7u);
  sVisual = SHOWDUINO_CARBIDE_VIS_IDENTIFY;
  Serial.println("[LAMP] PIXEL IDENTIFY 0");
  renderIdentify();
  return true;
}

void lampEngineStopIdentify() {
  if (sIdentify < 0) return;
  sIdentify = -1;
  if (!sEmergency && sMach.state == SHOWDUINO_CARBIDE_OFF) clearScratch();
}

int lampEngineIdentifyPixel() { return sIdentify; }
bool lampEngineIdentifyActive() { return sIdentify >= 0; }

uint16_t lampEngineCount() { return SHOWDUINO_LAMP_PIXEL_COUNT; }
int lampEnginePin() { return SHOWDUINO_LAMP_PIXEL_PIN; }
bool lampEngineJewelReady() { return sStrip != nullptr; }

void lampEnginePixelRgb(uint8_t i, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (i >= SHOWDUINO_LAMP_PIXEL_COUNT) return;
  if (r) *r = sRgb[i][0];
  if (g) *g = sRgb[i][1];
  if (b) *b = sRgb[i][2];
}
