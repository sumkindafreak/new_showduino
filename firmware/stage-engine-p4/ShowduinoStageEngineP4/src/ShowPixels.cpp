#include "ShowPixels.h"

#if SHOWDUINO_SHOW_PIXEL_ENABLED

#include <math.h>
#include "soc/soc_caps.h"
#if SOC_RMT_SUPPORTED
#include "esp32-hal-rmt.h"
#endif

static bool sReady = false;
static bool sEmergency = false;
static bool sTestActive = false;
static uint32_t sTestStartedMs = 0;
static uint32_t sLastFrameMs = 0;
static uint32_t sLastEmergencyRefreshMs = 0;
static uint8_t sGlobalBrightness = SHOWDUINO_SHOW_PIXEL_BRIGHTNESS;
static ShowduinoPixelSegmentState sSegments[SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS];
static uint8_t *sFrame = nullptr; /* RGB triplets */
#if SOC_RMT_SUPPORTED
static rmt_data_t *sItems = nullptr;
#endif
static size_t sSymbolCount = 0;

static uint8_t scale8(uint8_t value, uint8_t a, uint8_t b) {
  return (uint8_t)(((uint32_t)value * (uint32_t)a * (uint32_t)b) / (255UL * 255UL));
}

static uint8_t effectScale(uint8_t value, uint8_t factor) {
  return (uint8_t)(((uint16_t)value * factor) / 255U);
}

static void clearFrame() {
  if (sFrame) memset(sFrame, 0, (size_t)SHOWDUINO_SHOW_PIXEL_COUNT * 3U);
}

static void setPixelRaw(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (!sFrame || index >= SHOWDUINO_SHOW_PIXEL_COUNT) return;
  const size_t base = (size_t)index * 3U;
  sFrame[base + 0] = r;
  sFrame[base + 1] = g;
  sFrame[base + 2] = b;
}

static void setPixelScaled(uint16_t index, const ShowduinoPixelSegmentState &seg,
                           ShowduinoPixelColor color, uint8_t fxScale = 255) {
  uint8_t effective = effectScale(seg.brightness, fxScale);
  setPixelRaw(index,
              scale8(color.r, effective, sGlobalBrightness),
              scale8(color.g, effective, sGlobalBrightness),
              scale8(color.b, effective, sGlobalBrightness));
}

static void fillSegment(const ShowduinoPixelSegmentState &seg,
                        ShowduinoPixelColor color, uint8_t fxScale = 255) {
  const uint16_t end = min<uint32_t>((uint32_t)seg.start + seg.count,
                                     SHOWDUINO_SHOW_PIXEL_COUNT);
  for (uint16_t i = seg.start; i < end; ++i) setPixelScaled(i, seg, color, fxScale);
}

static ShowduinoPixelColor mixColor(ShowduinoPixelColor a, ShowduinoPixelColor b, uint8_t amount) {
  ShowduinoPixelColor out;
  out.r = (uint8_t)(((uint16_t)a.r * (255U - amount) + (uint16_t)b.r * amount) / 255U);
  out.g = (uint8_t)(((uint16_t)a.g * (255U - amount) + (uint16_t)b.g * amount) / 255U);
  out.b = (uint8_t)(((uint16_t)a.b * (255U - amount) + (uint16_t)b.b * amount) / 255U);
  return out;
}

static ShowduinoPixelColor hsv(uint16_t hue, uint8_t sat, uint8_t val) {
  hue %= 360U;
  const uint8_t region = hue / 60U;
  const uint16_t remainder = (hue % 60U) * 255U / 60U;
  const uint8_t p = (uint8_t)((uint16_t)val * (255U - sat) / 255U);
  const uint8_t q = (uint8_t)((uint16_t)val * (255U - ((uint16_t)sat * remainder / 255U)) / 255U);
  const uint8_t t = (uint8_t)((uint16_t)val * (255U - ((uint16_t)sat * (255U - remainder) / 255U)) / 255U);
  switch (region) {
    case 0: return {val, t, p};
    case 1: return {q, val, p};
    case 2: return {p, val, t};
    case 3: return {p, q, val};
    case 4: return {t, p, val};
    default: return {val, p, q};
  }
}

#if SOC_RMT_SUPPORTED
static void buildSymbolsFromFrame() {
  if (!sFrame || !sItems) return;
  size_t item = 0;
  for (uint16_t led = 0; led < SHOWDUINO_SHOW_PIXEL_COUNT; ++led) {
    const size_t base = (size_t)led * 3U;
    const uint8_t bytes[3] = {sFrame[base + 1], sFrame[base + 0], sFrame[base + 2]}; /* GRB */
    for (uint8_t bi = 0; bi < 3; ++bi) {
      uint8_t v = bytes[bi];
      for (int bit = 7; bit >= 0; --bit) {
        if (v & (1U << bit)) {
          sItems[item].level0 = 1;
          sItems[item].duration0 = 8;
          sItems[item].level1 = 0;
          sItems[item].duration1 = 4;
        } else {
          sItems[item].level0 = 1;
          sItems[item].duration0 = 4;
          sItems[item].level1 = 0;
          sItems[item].duration1 = 8;
        }
        ++item;
      }
    }
  }
  sSymbolCount = item;
}
#endif

static bool writeFrame() {
#if !SOC_RMT_SUPPORTED
  return false;
#else
  if (!sReady || !sItems || !sFrame) return false;
  buildSymbolsFromFrame();
  const bool ok = rmtWrite(SHOWDUINO_SHOW_PIXEL_PIN, sItems, sSymbolCount, 80);
  if (!ok) Serial.println("[PIXEL] Show line RMT write failed or timed out");
  return ok;
#endif
}

static uint32_t periodFromSpeed(uint8_t speed, uint32_t slowMs, uint32_t fastMs) {
  speed = constrain(speed, 1, 100);
  return slowMs - ((slowMs - fastMs) * (uint32_t)(speed - 1U) / 99UL);
}

static uint8_t triangleWave(uint32_t phase, uint32_t period) {
  if (period < 2) return 255;
  uint32_t p = phase % period;
  const uint32_t half = period / 2U;
  if (p <= half) return (uint8_t)(p * 255UL / max<uint32_t>(1, half));
  return (uint8_t)((period - p) * 255UL / max<uint32_t>(1, period - half));
}

static bool segmentExpired(ShowduinoPixelSegmentState &seg, uint32_t now) {
  if (!seg.durationMs) return false;
  if ((now - seg.startedMs) < seg.durationMs) return false;
  seg.active = false;
  return true;
}

static void renderSegment(ShowduinoPixelSegmentState &seg, uint32_t now) {
  if (!seg.active || !seg.configured || seg.count == 0) return;
  if (segmentExpired(seg, now)) return;

  const uint32_t elapsed = now - seg.startedMs;
  const uint16_t end = min<uint32_t>((uint32_t)seg.start + seg.count,
                                     SHOWDUINO_SHOW_PIXEL_COUNT);
  const uint16_t actualCount = (end > seg.start) ? (end - seg.start) : 0;
  if (!actualCount) return;

  switch (seg.effect) {
    case ShowduinoPixelFx::Off:
      break;

    case ShowduinoPixelFx::Solid:
      fillSegment(seg, seg.primary);
      break;

    case ShowduinoPixelFx::FadeIn: {
      const uint32_t duration = seg.durationMs ? seg.durationMs : periodFromSpeed(seg.speed, 5000, 250);
      uint8_t amount = (uint8_t)min<uint32_t>(255UL, elapsed * 255UL / max<uint32_t>(1, duration));
      fillSegment(seg, seg.primary, amount);
      break;
    }

    case ShowduinoPixelFx::FadeOut: {
      const uint32_t duration = seg.durationMs ? seg.durationMs : periodFromSpeed(seg.speed, 5000, 250);
      uint8_t amount = (elapsed >= duration) ? 0 : (uint8_t)(255UL - elapsed * 255UL / max<uint32_t>(1, duration));
      fillSegment(seg, seg.primary, amount);
      break;
    }

    case ShowduinoPixelFx::Pulse: {
      uint32_t period = periodFromSpeed(seg.speed, 5000, 250);
      fillSegment(seg, seg.primary, triangleWave(elapsed, period));
      break;
    }

    case ShowduinoPixelFx::Breathe: {
      const uint32_t period = periodFromSpeed(seg.speed, 7000, 700);
      const float angle = ((float)(elapsed % period) / (float)period) * 6.2831853f;
      const float shaped = (1.0f - cosf(angle)) * 0.5f;
      fillSegment(seg, seg.primary, (uint8_t)(shaped * 255.0f));
      break;
    }

    case ShowduinoPixelFx::Flicker: {
      const uint32_t interval = periodFromSpeed(seg.speed, 140, 20);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        const uint8_t floorV = (uint8_t)(255U - ((uint16_t)seg.intensity * 180U / 100U));
        seg.aux = (uint32_t)random(floorV, 256);
      }
      fillSegment(seg, seg.primary, (uint8_t)seg.aux);
      break;
    }

    case ShowduinoPixelFx::Candle: {
      const uint32_t interval = periodFromSpeed(seg.speed, 180, 35);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = (uint32_t)random(30, 120);
      }
      ShowduinoPixelColor warm = seg.secondary;
      if (warm.r == 0 && warm.g == 0 && warm.b == 0) warm = {255, 70, 0};
      ShowduinoPixelColor c = mixColor(seg.primary, warm, (uint8_t)seg.aux);
      fillSegment(seg, c, (uint8_t)random(190, 256));
      break;
    }

    case ShowduinoPixelFx::Fire: {
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint8_t heat = (uint8_t)random(120, 256);
        uint8_t red = heat;
        uint8_t green = (uint8_t)((uint16_t)heat * random(20, 65) / 100U);
        uint8_t blue = (uint8_t)random(0, 8);
        setPixelScaled(seg.start + p, seg, {red, green, blue});
      }
      break;
    }

    case ShowduinoPixelFx::Lightning: {
      const uint32_t interval = periodFromSpeed(seg.speed, 900, 80);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        const long chance = random(0, 101);
        seg.aux = (chance <= seg.randomness) ? (uint32_t)random(1, 4) : 0;
      }
      if (seg.aux > 0) {
        fillSegment(seg, seg.primary, 255);
        if (random(0, 100) < 45) --seg.aux;
      }
      break;
    }

    case ShowduinoPixelFx::Strobe: {
      const uint32_t period = periodFromSpeed(seg.speed, 1000, 60);
      if ((elapsed % period) < (period / 2U)) fillSegment(seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::RandomStrobe: {
      const uint32_t interval = periodFromSpeed(seg.speed, 700, 45);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = random(0, 101) <= seg.randomness ? 1 : 0;
      }
      if (seg.aux) fillSegment(seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Chase: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 350, 20);
      uint32_t step = elapsed / max<uint32_t>(1, stepMs);
      uint16_t pos = (uint16_t)(step % actualCount);
      if (seg.reverse) pos = actualCount - 1U - pos;
      uint16_t width = max<uint16_t>(1, (uint16_t)seg.intensity * min<uint16_t>(actualCount, 8) / 100U);
      for (uint16_t w = 0; w < width; ++w) setPixelScaled(seg.start + ((pos + w) % actualCount), seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Bounce: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 350, 20);
      const uint32_t span = actualCount <= 1 ? 1 : (uint32_t)(actualCount - 1U) * 2UL;
      uint32_t x = (elapsed / max<uint32_t>(1, stepMs)) % span;
      uint16_t pos = x < actualCount ? (uint16_t)x : (uint16_t)(span - x);
      if (seg.reverse) pos = actualCount - 1U - pos;
      setPixelScaled(seg.start + pos, seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Comet: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 300, 20);
      uint16_t head = (uint16_t)((elapsed / max<uint32_t>(1, stepMs)) % actualCount);
      if (seg.reverse) head = actualCount - 1U - head;
      uint16_t tail = max<uint16_t>(2, min<uint16_t>(actualCount, (uint16_t)(2U + seg.intensity / 12U)));
      for (uint16_t t = 0; t < tail; ++t) {
        int32_t pos = seg.reverse ? (int32_t)head + t : (int32_t)head - t;
        while (pos < 0) pos += actualCount;
        pos %= actualCount;
        uint8_t level = (uint8_t)(255U - ((uint32_t)t * 220U / tail));
        setPixelScaled(seg.start + (uint16_t)pos, seg, seg.primary, level);
      }
      break;
    }

    case ShowduinoPixelFx::Wipe:
    case ShowduinoPixelFx::ReverseWipe:
    case ShowduinoPixelFx::Build: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 300, 15);
      uint16_t filled = min<uint32_t>(actualCount, elapsed / max<uint32_t>(1, stepMs) + 1U);
      for (uint16_t p = 0; p < filled; ++p) {
        uint16_t pos = (seg.effect == ShowduinoPixelFx::ReverseWipe || seg.reverse)
                         ? (actualCount - 1U - p) : p;
        setPixelScaled(seg.start + pos, seg, seg.primary);
      }
      break;
    }

    case ShowduinoPixelFx::Sparkle: {
      fillSegment(seg, seg.secondary, 80);
      uint16_t sparks = max<uint16_t>(1, (uint16_t)actualCount * seg.intensity / 100U / 4U);
      for (uint16_t i = 0; i < sparks; ++i) {
        uint16_t pos = (uint16_t)random(0, actualCount);
        setPixelScaled(seg.start + pos, seg, seg.primary, (uint8_t)random(170, 256));
      }
      break;
    }

    case ShowduinoPixelFx::Twinkle: {
      const uint32_t period = periodFromSpeed(seg.speed, 4500, 400);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t shifted = elapsed + (uint32_t)p * 173UL;
        uint8_t level = triangleWave(shifted, period);
        setPixelScaled(seg.start + p, seg, seg.primary, level);
      }
      break;
    }

    case ShowduinoPixelFx::Glitch: {
      const uint32_t interval = periodFromSpeed(seg.speed, 500, 35);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = random(0, 0x7fffffff);
      }
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t x = seg.aux ^ ((uint32_t)p * 2654435761UL);
        if ((x % 100U) < seg.intensity) setPixelScaled(seg.start + p, seg, (x & 1U) ? seg.primary : seg.secondary);
      }
      break;
    }

    case ShowduinoPixelFx::Warning: {
      ShowduinoPixelColor warning = seg.primary;
      if (warning.r == 255 && warning.g == 255 && warning.b == 255) warning = {255, 0, 0};
      const uint32_t period = periodFromSpeed(seg.speed, 1800, 200);
      fillSegment(seg, warning, triangleWave(elapsed, period));
      break;
    }

    case ShowduinoPixelFx::Portal: {
      const uint32_t period = periodFromSpeed(seg.speed, 4000, 350);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t shifted = elapsed + ((uint32_t)p * period / max<uint16_t>(1, actualCount));
        uint8_t level = triangleWave(shifted, period);
        ShowduinoPixelColor c = mixColor(seg.secondary, seg.primary, level);
        setPixelScaled(seg.start + p, seg, c);
      }
      break;
    }

    case ShowduinoPixelFx::Rainbow: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 80, 5);
      uint16_t baseHue = (uint16_t)((elapsed / max<uint32_t>(1, stepMs)) % 360U);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint16_t hue = (uint16_t)((baseHue + ((uint32_t)p * 360U / actualCount)) % 360U);
        setPixelScaled(seg.start + p, seg, hsv(hue, 255, 255));
      }
      break;
    }

    case ShowduinoPixelFx::CustomSequence: {
      const uint32_t unit = periodFromSpeed(seg.speed, 1600, 150);
      uint32_t step = (elapsed / max<uint32_t>(1, unit)) % 3U;
      if (step == 0) fillSegment(seg, seg.primary);
      else if (step == 1) fillSegment(seg, seg.secondary);
      break;
    }

    default:
      break;
  }
}

static void renderEmergencyWhite() {
  if (!sFrame) return;
  const uint8_t v = sGlobalBrightness;
  for (uint16_t i = 0; i < SHOWDUINO_SHOW_PIXEL_COUNT; ++i) setPixelRaw(i, v, v, v);
}

static void renderCommissioningTest(uint32_t now) {
  const uint32_t elapsed = now - sTestStartedMs;
  const uint32_t block = 700UL;
  clearFrame();
  if (elapsed < block) {
    for (uint16_t i = 0; i < SHOWDUINO_SHOW_PIXEL_COUNT; ++i) setPixelRaw(i, sGlobalBrightness, 0, 0);
  } else if (elapsed < block * 2UL) {
    for (uint16_t i = 0; i < SHOWDUINO_SHOW_PIXEL_COUNT; ++i) setPixelRaw(i, 0, sGlobalBrightness, 0);
  } else if (elapsed < block * 3UL) {
    for (uint16_t i = 0; i < SHOWDUINO_SHOW_PIXEL_COUNT; ++i) setPixelRaw(i, 0, 0, sGlobalBrightness);
  } else if (elapsed < block * 4UL) {
    for (uint16_t i = 0; i < SHOWDUINO_SHOW_PIXEL_COUNT; ++i) setPixelRaw(i, sGlobalBrightness, sGlobalBrightness, sGlobalBrightness);
  } else {
    const uint32_t chaseElapsed = elapsed - block * 4UL;
    const uint32_t chaseLength = (uint32_t)SHOWDUINO_SHOW_PIXEL_COUNT * 30UL;
    if (chaseElapsed >= chaseLength) {
      sTestActive = false;
      clearFrame();
      Serial.println("[PIXEL] Commissioning test complete");
      return;
    }
    uint16_t pos = (uint16_t)(chaseElapsed / 30UL);
    if (pos < SHOWDUINO_SHOW_PIXEL_COUNT) setPixelRaw(pos, sGlobalBrightness, sGlobalBrightness, sGlobalBrightness);
  }
}

bool showPixelsBegin() {
  if (sReady) return true;
#if !SOC_RMT_SUPPORTED
  Serial.println("[PIXEL] RMT not supported — P4 show pixel line disabled");
  return false;
#else
  for (uint8_t i = 0; i < SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS; ++i) sSegments[i] = showduinoPixelDefaultSegment();

  sFrame = (uint8_t *)calloc((size_t)SHOWDUINO_SHOW_PIXEL_COUNT * 3U, 1);
  const size_t bits = (size_t)SHOWDUINO_SHOW_PIXEL_COUNT * 24U;
  sItems = (rmt_data_t *)malloc(bits * sizeof(rmt_data_t));
  if (!sFrame || !sItems) {
    Serial.println("[PIXEL] Show line allocation failed");
    free(sFrame); sFrame = nullptr;
    free(sItems); sItems = nullptr;
    return false;
  }

  pinMode(SHOWDUINO_SHOW_PIXEL_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_SHOW_PIXEL_PIN, LOW);
  bool inited = rmtInit(SHOWDUINO_SHOW_PIXEL_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_2, 10000000);
  if (!inited) inited = rmtInit(SHOWDUINO_SHOW_PIXEL_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
  if (!inited) {
    Serial.printf("[PIXEL] Show line RMT init failed on GPIO %d\n", SHOWDUINO_SHOW_PIXEL_PIN);
    free(sFrame); sFrame = nullptr;
    free(sItems); sItems = nullptr;
    return false;
  }
  rmtSetEOT(SHOWDUINO_SHOW_PIXEL_PIN, 0);
  sReady = true;
  clearFrame();
  writeFrame();
  Serial.printf("[PIXEL] Show line ready GPIO=%d count=%u segments=%u data-resistor=%uR\n",
                SHOWDUINO_SHOW_PIXEL_PIN,
                (unsigned)SHOWDUINO_SHOW_PIXEL_COUNT,
                (unsigned)SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS,
                (unsigned)SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS);
  return true;
#endif
}

void showPixelsBlackout() {
  sTestActive = false;
  for (uint8_t i = 0; i < SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS; ++i) {
    sSegments[i].active = false;
    sSegments[i].effect = ShowduinoPixelFx::Off;
  }
  if (!sReady && !showPixelsBegin()) return;
  clearFrame();
  writeFrame();
}

void showPixelsOnEmergency(bool active) {
  if (!sReady && !showPixelsBegin()) return;
  sEmergency = active;
  sTestActive = false;
  if (active) {
    renderEmergencyWhite();
    writeFrame();
    sLastEmergencyRefreshMs = millis();
    Serial.println("[PIXEL] GLOBAL EMERGENCY OVERRIDE → ALL P4 SHOW PIXELS WHITE");
  } else {
    for (uint8_t i = 0; i < SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS; ++i) sSegments[i].active = false;
    clearFrame();
    writeFrame();
    Serial.println("[PIXEL] Emergency override cleared → show pixels remain OFF");
  }
}

void showPixelsService() {
  if (!sReady) return;
  const uint32_t now = millis();

  if (sEmergency) {
    if ((now - sLastEmergencyRefreshMs) >= 250UL) {
      sLastEmergencyRefreshMs = now;
      renderEmergencyWhite();
      writeFrame();
    }
    return;
  }

  if ((now - sLastFrameMs) < SHOWDUINO_SHOW_PIXEL_FRAME_MS) return;
  sLastFrameMs = now;

  if (sTestActive) {
    renderCommissioningTest(now);
    writeFrame();
    return;
  }

  clearFrame();
  for (uint8_t i = 0; i < SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS; ++i) renderSegment(sSegments[i], now);
  writeFrame();
}

bool showPixelsReady() { return sReady; }
bool showPixelsEmergencyOverride() { return sEmergency; }
uint16_t showPixelsCount() { return SHOWDUINO_SHOW_PIXEL_COUNT; }
uint8_t showPixelsGlobalBrightness() { return sGlobalBrightness; }

void showPixelsPrintStatus() {
  Serial.printf("[PIXEL] SHOW GPIO=%d count=%u ready=%s brightness=%u emergency=%s\n",
                SHOWDUINO_SHOW_PIXEL_PIN,
                (unsigned)SHOWDUINO_SHOW_PIXEL_COUNT,
                sReady ? "YES" : "NO",
                (unsigned)sGlobalBrightness,
                sEmergency ? "WHITE_OVERRIDE" : "CLEAR");
  for (uint8_t i = 0; i < SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS; ++i) {
    const ShowduinoPixelSegmentState &seg = sSegments[i];
    if (!seg.configured) continue;
    Serial.printf("[PIXEL] SEG %u start=%u count=%u fx=%s active=%s bright=%u speed=%u intensity=%u randomness=%u\n",
                  (unsigned)i, (unsigned)seg.start, (unsigned)seg.count,
                  showduinoPixelFxName(seg.effect), seg.active ? "YES" : "NO",
                  (unsigned)seg.brightness, (unsigned)seg.speed,
                  (unsigned)seg.intensity, (unsigned)seg.randomness);
  }
}

static uint8_t splitTokens(const String &in, String *parts, uint8_t maxParts) {
  uint8_t count = 0;
  int from = 0;
  while (from <= (int)in.length() && count < maxParts) {
    int colon = in.indexOf(':', from);
    if (colon < 0) colon = in.length();
    parts[count++] = in.substring(from, colon);
    from = colon + 1;
    if (colon >= (int)in.length()) break;
  }
  return count;
}

static bool parseU8(const String &s, uint8_t *out, uint16_t maxValue = 255) {
  if (!out || s.length() == 0) return false;
  for (unsigned i = 0; i < s.length(); ++i) if (!isDigit(s[i])) return false;
  long v = s.toInt();
  if (v < 0 || v > maxValue) return false;
  *out = (uint8_t)v;
  return true;
}

static bool parseU16(const String &s, uint16_t *out) {
  if (!out || s.length() == 0) return false;
  for (unsigned i = 0; i < s.length(); ++i) if (!isDigit(s[i])) return false;
  long v = s.toInt();
  if (v < 0 || v > 65535) return false;
  *out = (uint16_t)v;
  return true;
}

static bool parseU32(const String &s, uint32_t *out) {
  if (!out || s.length() == 0) return false;
  for (unsigned i = 0; i < s.length(); ++i) if (!isDigit(s[i])) return false;
  *out = (uint32_t)strtoul(s.c_str(), nullptr, 10);
  return true;
}

static void setReply(char *reply, size_t len, const String &value) {
  if (!reply || !len) return;
  snprintf(reply, len, "%s", value.c_str());
}

bool showPixelsHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (reply && replyLen) reply[0] = '\0';
  if (!command || strncmp(command, "PIXEL:", 6) != 0) return false;
  if (!sReady && !showPixelsBegin()) {
    setReply(reply, replyLen, "PIXEL:ERROR:NOT_READY");
    return true;
  }

  String cmd(command);
  String p[12];
  uint8_t n = splitTokens(cmd, p, 12);

  if (cmd == "PIXEL:STATUS") {
    showPixelsPrintStatus();
    char line[128];
    snprintf(line, sizeof(line), "PIXEL:STATUS:%s:GPIO=%d:COUNT=%u:BRIGHTNESS=%u:EMERGENCY=%u",
             sReady ? "READY" : "FAULT", SHOWDUINO_SHOW_PIXEL_PIN,
             (unsigned)SHOWDUINO_SHOW_PIXEL_COUNT, (unsigned)sGlobalBrightness,
             sEmergency ? 1U : 0U);
    setReply(reply, replyLen, line);
    return true;
  }

  if (sEmergency) {
    setReply(reply, replyLen, "PIXEL:REJECTED:EMERGENCY_ACTIVE");
    return true;
  }

  if (cmd == "PIXEL:OFF" || cmd == "PIXEL:BLACKOUT") {
    showPixelsBlackout();
    setReply(reply, replyLen, "PIXEL:OFF:OK");
    return true;
  }

  if (cmd == "PIXEL:TEST") {
    sTestActive = true;
    sTestStartedMs = millis();
    setReply(reply, replyLen, "PIXEL:TEST:STARTED");
    return true;
  }
  if (cmd == "PIXEL:TEST:STOP") {
    sTestActive = false;
    showPixelsBlackout();
    setReply(reply, replyLen, "PIXEL:TEST:STOPPED");
    return true;
  }

  if (n == 3 && p[1] == "BRIGHTNESS") {
    uint8_t value;
    if (!parseU8(p[2], &value)) setReply(reply, replyLen, "PIXEL:ERROR:BRIGHTNESS");
    else {
      sGlobalBrightness = value;
      setReply(reply, replyLen, "PIXEL:BRIGHTNESS:OK");
    }
    return true;
  }

  if (n == 5 && p[1] == "SOLID") {
    uint8_t r, g, b;
    if (!parseU8(p[2], &r) || !parseU8(p[3], &g) || !parseU8(p[4], &b)) {
      setReply(reply, replyLen, "PIXEL:ERROR:COLOR");
      return true;
    }
    ShowduinoPixelSegmentState &seg = sSegments[0];
    seg = showduinoPixelDefaultSegment();
    seg.configured = true; seg.active = true; seg.start = 0; seg.count = SHOWDUINO_SHOW_PIXEL_COUNT;
    seg.effect = ShowduinoPixelFx::Solid; seg.primary = {r, g, b}; seg.startedMs = millis();
    setReply(reply, replyLen, "PIXEL:SOLID:OK");
    return true;
  }

  if (n < 4 || p[1] != "SEGMENT") {
    setReply(reply, replyLen, "PIXEL:ERROR:UNKNOWN_COMMAND");
    return true;
  }

  uint8_t segId;
  if (!parseU8(p[2], &segId, SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS - 1U)) {
    setReply(reply, replyLen, "PIXEL:ERROR:SEGMENT_ID");
    return true;
  }
  ShowduinoPixelSegmentState &seg = sSegments[segId];
  const String &op = p[3];

  if (op == "STATUS" && n == 4) {
    char line[160];
    snprintf(line, sizeof(line), "PIXEL:SEGMENT:%u:STATUS:START=%u:COUNT=%u:FX=%s:ACTIVE=%u",
             (unsigned)segId, (unsigned)seg.start, (unsigned)seg.count,
             showduinoPixelFxName(seg.effect), seg.active ? 1U : 0U);
    setReply(reply, replyLen, line);
    return true;
  }

  if (op == "RANGE" && n == 6) {
    uint16_t start, count;
    if (!parseU16(p[4], &start) || !parseU16(p[5], &count) || count == 0 ||
        start >= SHOWDUINO_SHOW_PIXEL_COUNT || (uint32_t)start + count > SHOWDUINO_SHOW_PIXEL_COUNT) {
      setReply(reply, replyLen, "PIXEL:ERROR:RANGE");
      return true;
    }
    if (!seg.configured) seg = showduinoPixelDefaultSegment();
    seg.configured = true; seg.start = start; seg.count = count;
    setReply(reply, replyLen, "PIXEL:SEGMENT:RANGE:OK");
    return true;
  }

  if (op == "FX" && n == 5) {
    ShowduinoPixelFx fx;
    String name = p[4]; name.toUpperCase();
    if (!showduinoPixelFxFromName(name.c_str(), &fx)) {
      setReply(reply, replyLen, "PIXEL:ERROR:FX");
      return true;
    }
    if (!seg.configured) seg = showduinoPixelDefaultSegment();
    seg.configured = true; seg.effect = fx;
    setReply(reply, replyLen, "PIXEL:SEGMENT:FX:OK");
    return true;
  }

  if ((op == "COLOR" || op == "COLOR2") && n == 7) {
    uint8_t r, g, b;
    if (!parseU8(p[4], &r) || !parseU8(p[5], &g) || !parseU8(p[6], &b)) {
      setReply(reply, replyLen, "PIXEL:ERROR:COLOR");
      return true;
    }
    if (!seg.configured) seg = showduinoPixelDefaultSegment();
    seg.configured = true;
    if (op == "COLOR") seg.primary = {r, g, b}; else seg.secondary = {r, g, b};
    setReply(reply, replyLen, "PIXEL:SEGMENT:COLOR:OK");
    return true;
  }

  if ((op == "BRIGHTNESS" || op == "SPEED" || op == "INTENSITY" || op == "RANDOMNESS") && n == 5) {
    uint8_t value;
    uint16_t maxV = (op == "BRIGHTNESS") ? 255 : 100;
    if (!parseU8(p[4], &value, maxV) || ((op == "SPEED") && value == 0)) {
      setReply(reply, replyLen, "PIXEL:ERROR:PARAMETER");
      return true;
    }
    if (!seg.configured) seg = showduinoPixelDefaultSegment();
    seg.configured = true;
    if (op == "BRIGHTNESS") seg.brightness = value;
    else if (op == "SPEED") seg.speed = value;
    else if (op == "INTENSITY") seg.intensity = value;
    else seg.randomness = value;
    setReply(reply, replyLen, "PIXEL:SEGMENT:PARAMETER:OK");
    return true;
  }

  if (op == "DURATION" && n == 5) {
    uint32_t value;
    if (!parseU32(p[4], &value)) setReply(reply, replyLen, "PIXEL:ERROR:DURATION");
    else {
      if (!seg.configured) seg = showduinoPixelDefaultSegment();
      seg.configured = true; seg.durationMs = value;
      setReply(reply, replyLen, "PIXEL:SEGMENT:DURATION:OK");
    }
    return true;
  }

  if (op == "REVERSE" && n == 5) {
    if (p[4] != "0" && p[4] != "1") setReply(reply, replyLen, "PIXEL:ERROR:REVERSE");
    else {
      if (!seg.configured) seg = showduinoPixelDefaultSegment();
      seg.configured = true; seg.reverse = (p[4] == "1");
      setReply(reply, replyLen, "PIXEL:SEGMENT:REVERSE:OK");
    }
    return true;
  }

  if (op == "START" && n == 4) {
    if (!seg.configured || seg.count == 0) setReply(reply, replyLen, "PIXEL:ERROR:SEGMENT_NOT_CONFIGURED");
    else {
      seg.active = true; seg.startedMs = millis(); seg.lastStepMs = 0; seg.phase = 0; seg.aux = 0;
      setReply(reply, replyLen, "PIXEL:SEGMENT:START:OK");
    }
    return true;
  }

  if (op == "STOP" && n == 4) {
    seg.active = false;
    setReply(reply, replyLen, "PIXEL:SEGMENT:STOP:OK");
    return true;
  }

  setReply(reply, replyLen, "PIXEL:ERROR:UNKNOWN_SEGMENT_COMMAND");
  return true;
}

#endif /* SHOWDUINO_SHOW_PIXEL_ENABLED */
