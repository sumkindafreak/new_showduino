#include "MosfetIdentifierPixels.h"
#include "MosfetOutputEngine.h"
#include "MosfetNodeState.h"
#include "EspNowMosfetTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_mosfet_identifier.h"
#include "../../shared-node/NodeSoftAp.h"

#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

/*
 * Priority (highest first):
 *   EMERGENCY             → solid full-bright white (MOSFET outputs remain OFF)
 *   FAULT                 → magenta ↔ amber
 *   IDENTIFY              → white chase ~5 s
 *   ACTIVE MOSFET OUT     → green @ actual duty
 *   NODE STATUS / WIFI    → odd-colour indication on OFF channels
 *
 * GREEN is reserved exclusively for energised MOSFET outputs.
 */

static Adafruit_NeoPixel *sStrip = nullptr;
static bool sReady = false;
static bool sForcedOff = false;

static ShowduinoMosfetRgb sLastRgb[4];
static bool sLastValid = false;

static uint32_t sIdentifyUntil = 0;
static uint32_t sAnimStepMs = 0;
static uint8_t sAnimStep = 0;

enum AnimMode : uint8_t {
  ANIM_NONE = 0,
  ANIM_BOOT,
  ANIM_GRANT,
  ANIM_WEBUI,
  ANIM_FAULT,
};
static AnimMode sAnim = ANIM_NONE;
static uint8_t sAnimPhase = 0; /* WebUI: 0 magenta, 1 off, 2 magenta, 3 done */
static uint8_t sLastStations = 0;
static int8_t sSearchPos = 0;
static int8_t sSearchDir = 1;
static uint32_t sSearchStepMs = 0;
static uint32_t sHeartbeatMs = 0;
static uint8_t sHeartbeatPhase = 0;

static uint32_t colorOf(ShowduinoMosfetRgb c) {
  return sStrip->Color(c.r, c.g, c.b);
}

static void invalidateCache() { sLastValid = false; }

static void pushRgb(const ShowduinoMosfetRgb rgb[4], bool force) {
  if (!sReady || !sStrip) return;
  bool changed = force || !sLastValid;
  if (!changed) {
    for (uint8_t i = 0; i < 4; ++i) {
      if (sLastRgb[i].r != rgb[i].r || sLastRgb[i].g != rgb[i].g ||
          sLastRgb[i].b != rgb[i].b) {
        changed = true;
        break;
      }
    }
  }
  if (!changed) return;
  for (uint8_t i = 0; i < 4; ++i) {
    sLastRgb[i] = rgb[i];
    sStrip->setPixelColor(i, colorOf(rgb[i]));
  }
  sLastValid = true;
  sStrip->show();
}

static void fillOff(ShowduinoMosfetRgb out[4]) {
  for (uint8_t i = 0; i < 4; ++i) out[i] = showduino_mosfet_rgb_off();
}

static uint8_t statusBrightNow() {
  /* Slow breathe 8 ↔ 12 so idle status never becomes a Christmas tree. */
  const uint8_t base = SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS;
  const uint8_t lo = (base > 2) ? (uint8_t)(base - 2) : base;
  const uint8_t hi = (uint8_t)(base + 2);
  return (sHeartbeatPhase & 1u) ? hi : lo;
}

static void startAnim(AnimMode mode) {
  sAnim = mode;
  sAnimStep = 0;
  sAnimPhase = 0;
  sAnimStepMs = millis();
  invalidateCache();
}

void mosfetIdentifierPixelsSafeGpio() {
  pinMode(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO, OUTPUT);
  digitalWrite(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO, LOW);
}

void mosfetIdentifierPixelsAllOff() {
  sIdentifyUntil = 0;
  sAnim = ANIM_NONE;
  sForcedOff = true;
  invalidateCache();
  ShowduinoMosfetRgb off[4];
  fillOff(off);
  if (sReady && sStrip) {
    pushRgb(off, true);
  }
}

void mosfetIdentifierPixelsBegin() {
  sReady = false;
  sForcedOff = false;
  sIdentifyUntil = 0;
  sAnim = ANIM_NONE;
  sLastStations = 0;
  sSearchPos = 0;
  sSearchDir = 1;
  invalidateCache();
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
#if !SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_VERIFIED
  Serial.println("[MOSFET] Identifier pixels GPIO25 SOFTWARE DEFINED / HARDWARE UNVERIFIED");
#endif
  Serial.printf(
      "[MOSFET] Identifier pixels: %u × WS2812 GPIO%u — green=OUT duty; "
      "status purple/cyan/turquoise/violet @%u\n",
      (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_COUNT,
      (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO,
      (unsigned)SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS);
  /* BOOT: violet 1→2→3→4 once */
  startAnim(ANIM_BOOT);
}

void mosfetIdentifierPixelsIdentify() {
  if (mosfetNodeStateEmergency()) {
    mosfetIdentifierPixelsAllOff();
    return;
  }
  sForcedOff = false;
  sIdentifyUntil = millis() + SHOWDUINO_MOSFET_IDENTIFY_MS;
  sAnimStepMs = millis();
  sAnimStep = 0;
  sAnim = ANIM_NONE; /* IDENTIFY outranks transient anims */
  invalidateCache();
  Serial.println("[MOSFET] IDENTIFY — white chase (outputs unchanged)");
}

bool mosfetIdentifierPixelsReady() { return sReady; }
bool mosfetIdentifierPixelsIdentifying() {
  return sIdentifyUntil != 0 && (int32_t)(millis() - sIdentifyUntil) < 0;
}

static void paintIdentify(ShowduinoMosfetRgb out[4]) {
  const uint8_t w = SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS;
  fillOff(out);
  out[sAnimStep % 4] = showduino_mosfet_rgb_white(w);
}

static void paintFault(ShowduinoMosfetRgb out[4]) {
  const uint8_t b = (uint8_t)(SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS / 2u);
  const ShowduinoMosfetRgb c =
      (sAnimStep & 1u) ? showduino_mosfet_rgb_magenta(b)
                       : showduino_mosfet_rgb_amber(b);
  for (uint8_t i = 0; i < 4; ++i) out[i] = c;
}

static void paintBoot(ShowduinoMosfetRgb out[4]) {
  const uint8_t b = (uint8_t)(SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS * 3u);
  fillOff(out);
  if (sAnimStep < 4) out[sAnimStep] = showduino_mosfet_rgb_violet(b > 40 ? 40 : b);
}

static void paintGrant(ShowduinoMosfetRgb out[4]) {
  const uint8_t b = (uint8_t)(SHOWDUINO_MOSFET_IDENTIFIER_STATUS_BRIGHTNESS * 3u);
  fillOff(out);
  if (sAnimStep < 4) out[sAnimStep] = showduino_mosfet_rgb_turquoise(b > 40 ? 40 : b);
}

static void paintWebUi(ShowduinoMosfetRgb out[4], const uint8_t levels[4]) {
  /* Magenta → OFF → Magenta on channels that are not actively green. */
  fillOff(out);
  const bool on = (sAnimPhase == 0 || sAnimPhase == 2);
  if (!on) return;
  const uint8_t b = (uint8_t)(SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS / 2u);
  for (uint8_t i = 0; i < 4; ++i) {
    if (levels[i] == 0) out[i] = showduino_mosfet_rgb_magenta(b);
  }
}

static void composeSteady(ShowduinoMosfetRgb out[4], const uint8_t levels[4]) {
  ShowduinoMosfetIdentStatus st;
  st.softApUp = nodeSoftApStarted() ? 1 : 0;
  st.espNowLinked = mosfetEspNowLinkFresh() ? 1 : 0;
  st.searching = (!mosfetNodeStateOwned() &&
                  mosfetNodeStateGet() == SHOWDUINO_MOSFET_ST_SEARCHING)
                     ? 1
                     : 0;
  st.owned = mosfetNodeStateOwned() ? 1 : 0;
  st.healthy = (!mosfetNodeStateFault() && !mosfetNodeStateEmergency()) ? 1 : 0;
  st.fault = mosfetNodeStateFault() ? 1 : 0;
  st.statusBright = statusBrightNow();
  showduino_mosfet_identifier_compose(
      levels, SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS, &st, out);

  /* ESP-NOW searching: slow amber bounce on OFF pixels; SoftAP purple may remain. */
  if (st.searching) {
    const uint32_t now = millis();
    if ((now - sSearchStepMs) >= 450UL) {
      sSearchStepMs = now;
      sSearchPos = (int8_t)(sSearchPos + sSearchDir);
      if (sSearchPos >= 3) {
        sSearchPos = 3;
        sSearchDir = -1;
      } else if (sSearchPos <= 0) {
        sSearchPos = 0;
        sSearchDir = 1;
      }
    }
    for (uint8_t i = 0; i < 4; ++i) {
      if (levels[i] > 0) continue; /* green wins */
      if ((int8_t)i == sSearchPos) {
        out[i] = showduino_mosfet_rgb_amber(statusBrightNow());
      } else if (i == 0 && st.softApUp) {
        out[i] = showduino_mosfet_rgb_purple(statusBrightNow());
      } else {
        out[i] = showduino_mosfet_rgb_off();
      }
    }
  }
}

void mosfetIdentifierPixelsService() {
  if (mosfetNodeStateEmergency()) {
    sIdentifyUntil = 0;
    sAnim = ANIM_NONE;
    sForcedOff = false;
    ShowduinoMosfetRgb emergency[4];
    for (uint8_t i = 0; i < 4; ++i) emergency[i] = showduino_mosfet_rgb_white(255);
    pushRgb(emergency, false);
    return;
  }

  const uint32_t now = millis();
  if ((now - sHeartbeatMs) >= 800UL) {
    sHeartbeatMs = now;
    sHeartbeatPhase++;
  }

  /* Rising-edge WebUI client → magenta double-pulse (priority below IDENTIFY). */
  uint8_t stations = 0;
  if (nodeSoftApStarted()) {
    stations = (uint8_t)WiFi.softAPgetStationNum();
  }
  if (stations > sLastStations && stations > 0 && !mosfetIdentifierPixelsIdentifying() &&
      !mosfetNodeStateFault() && sAnim != ANIM_BOOT && sAnim != ANIM_GRANT) {
    startAnim(ANIM_WEBUI);
  }
  sLastStations = stations;

  /* P4 ownership grant → turquoise sweep once. */
  if (mosfetNodeStateConsumeEnteredShow() && !mosfetIdentifierPixelsIdentifying() &&
      !mosfetNodeStateFault()) {
    startAnim(ANIM_GRANT);
  }

  if (mosfetNodeStateFault()) {
    if (sAnim != ANIM_FAULT) startAnim(ANIM_FAULT);
  } else if (sAnim == ANIM_FAULT) {
    sAnim = ANIM_NONE;
  }

  if (!sReady) return;
  sForcedOff = false;

  uint8_t levels[4] = {0, 0, 0, 0};
  mosfetOutputEngineLevels(levels);
  ShowduinoMosfetRgb frame[4];
  fillOff(frame);

  /* --- priority stack --- */
  if (mosfetNodeStateFault() || sAnim == ANIM_FAULT) {
    if ((now - sAnimStepMs) >= 350UL) {
      sAnimStepMs = now;
      sAnimStep++;
    }
    paintFault(frame);
    pushRgb(frame, false);
    return;
  }

  if (sIdentifyUntil) {
    if ((int32_t)(now - sIdentifyUntil) >= 0) {
      sIdentifyUntil = 0;
      invalidateCache();
    } else {
      if ((now - sAnimStepMs) >= 200UL) {
        sAnimStepMs = now;
        sAnimStep = (uint8_t)((sAnimStep + 1) % 4);
      }
      paintIdentify(frame);
      pushRgb(frame, false);
      return;
    }
  }

  if (sAnim == ANIM_BOOT) {
    if ((now - sAnimStepMs) >= 180UL) {
      sAnimStepMs = now;
      sAnimStep++;
    }
    if (sAnimStep >= 4) {
      sAnim = ANIM_NONE;
      invalidateCache();
    } else {
      paintBoot(frame);
      pushRgb(frame, false);
      return;
    }
  }

  if (sAnim == ANIM_GRANT) {
    if ((now - sAnimStepMs) >= 120UL) {
      sAnimStepMs = now;
      sAnimStep++;
    }
    if (sAnimStep >= 4) {
      sAnim = ANIM_NONE;
      invalidateCache();
    } else {
      paintGrant(frame);
      pushRgb(frame, false);
      return;
    }
  }

  if (sAnim == ANIM_WEBUI) {
    if ((now - sAnimStepMs) >= 160UL) {
      sAnimStepMs = now;
      sAnimPhase++;
      if (sAnimPhase >= 3) {
        sAnim = ANIM_NONE;
        invalidateCache();
      }
    }
    if (sAnim == ANIM_WEBUI) {
      paintWebUi(frame, levels);
      /* Preserve green on active outs during WebUI flash. */
      for (uint8_t i = 0; i < 4; ++i) {
        if (levels[i] > 0)
          frame[i] = showduino_mosfet_rgb_green(
              levels[i], SHOWDUINO_MOSFET_IDENTIFIER_MAX_BRIGHTNESS);
      }
      pushRgb(frame, false);
      return;
    }
  }

  composeSteady(frame, levels);
  pushRgb(frame, false);
}
