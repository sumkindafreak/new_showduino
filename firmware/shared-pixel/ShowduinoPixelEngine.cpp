#include "ShowduinoPixelEngine.h"
/* BoardConfig.h + NodeConfig.h provided by the including sketch TU */
#include <math.h>
#ifndef SHOWDUINO_PIXEL_USE_FASTLED
#define SHOWDUINO_PIXEL_USE_FASTLED 0
#endif
#if SHOWDUINO_PIXEL_USE_FASTLED
#include <FastLED.h>
#include <new>
#ifndef SHOWDUINO_PIXEL_FASTLED_ORDER
#define SHOWDUINO_PIXEL_FASTLED_ORDER GRB
#endif
#else
#include <Adafruit_NeoPixel.h>
#endif

static bool sReady = false;
static bool sEmergency = false;
static bool sTestActive = false;
static bool sLocateActive = false;
static uint32_t sTestStartedMs = 0;
static uint32_t sLocateStartedMs = 0;
static uint32_t sLastFrameMs = 0;
static uint32_t sLastEmergencyRefreshMs = 0;
static uint8_t sGlobalBrightness = 255;
static uint16_t sConfiguredCount = 0;
static uint16_t sCount = 0;
static ShowduinoPixelSegmentState sSegments[SHOWDUINO_PIXEL_MAX_SEGMENTS];
static uint8_t *sFrame = nullptr;
#if SHOWDUINO_PIXEL_USE_FASTLED
static CRGB *sFastLeds = nullptr;
static CLEDController *sFastController = nullptr;
#else
static Adafruit_NeoPixel *sStrip = nullptr;
#endif





#include "ShowduinoPixelMath.inc"

static void clearFrame() {
  if (sFrame) memset(sFrame, 0, (size_t)sCount * 3U);
}

static void setPixelRaw(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  if (!sFrame || index >= sCount) return;
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
                                     sCount);
  for (uint16_t i = seg.start; i < end; ++i) setPixelScaled(i, seg, color, fxScale);
}





static bool writeFrame() {
  if (!sReady || !sFrame) return false;
#if SHOWDUINO_PIXEL_USE_FASTLED
  if (!sFastLeds || !sFastController) return false;
  for (uint16_t i = 0; i < sCount; ++i) {
    const size_t base = (size_t)i * 3U;
    sFastLeds[i] = CRGB(sFrame[base], sFrame[base + 1], sFrame[base + 2]);
  }
  // Showduino has already scaled colour by segment and global brightness.
  FastLED.show();
#else
  if (!sStrip) return false;
  for (uint16_t i = 0; i < sCount; ++i) {
    const size_t base = (size_t)i * 3U;
    sStrip->setPixelColor(i, sStrip->Color(sFrame[base], sFrame[base + 1], sFrame[base + 2]));
  }
  sStrip->show();
#endif
  return true;
}





static bool segmentExpired(ShowduinoPixelSegmentState &seg, uint32_t now) {
  if (!seg.durationMs) return false;
  if ((now - seg.startedMs) < seg.durationMs) return false;
  seg.active = false;
  return true;
}

#include "ShowduinoPixelEffects.inc"

static void renderEmergencyWhite() {
  if (!sFrame) return;
  for (uint16_t i = 0; i < sCount; ++i) setPixelRaw(i, 255, 255, 255);
}

static void renderCommissioningTest(uint32_t now) {
  const uint32_t elapsed = now - sTestStartedMs;
  const uint32_t block = 700UL;
  clearFrame();
  if (elapsed < block) {
    for (uint16_t i = 0; i < sCount; ++i) setPixelRaw(i, sGlobalBrightness, 0, 0);
  } else if (elapsed < block * 2UL) {
    for (uint16_t i = 0; i < sCount; ++i) setPixelRaw(i, 0, sGlobalBrightness, 0);
  } else if (elapsed < block * 3UL) {
    for (uint16_t i = 0; i < sCount; ++i) setPixelRaw(i, 0, 0, sGlobalBrightness);
  } else if (elapsed < block * 4UL) {
    for (uint16_t i = 0; i < sCount; ++i) setPixelRaw(i, sGlobalBrightness, sGlobalBrightness, sGlobalBrightness);
  } else {
    const uint32_t chaseElapsed = elapsed - block * 4UL;
    const uint32_t chaseLength = (uint32_t)sCount * 30UL;
    if (chaseElapsed >= chaseLength) {
      sTestActive = false;
      clearFrame();
      Serial.println("[PIXEL] Commissioning test complete");
      return;
    }
    uint16_t pos = (uint16_t)(chaseElapsed / 30UL);
    if (pos < sCount) setPixelRaw(pos, sGlobalBrightness, sGlobalBrightness, sGlobalBrightness);
  }
}

static void pixelEngineShutdown() {
  sLocateActive = false;
  sTestActive = false;
  if (sReady && sFrame) {
    clearFrame();
    writeFrame();
  }
  sReady = false;
  sCount = 0;
#if SHOWDUINO_PIXEL_USE_FASTLED
  // Retain FastLED's registered controller when PIXEL:INIT changes length.
  if (sFastController) sFastController->setLeds(nullptr, 0);
  delete[] sFastLeds;
  sFastLeds = nullptr;
#else
  if (sStrip) {
    delete sStrip;
    sStrip = nullptr;
  }
#endif
  free(sFrame);
  sFrame = nullptr;
}

bool pixelEngineSafeGpio() {
  pinMode(SHOWDUINO_PIXEL_DATA_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_PIXEL_DATA_PIN, LOW);
  return true;
}

bool pixelEngineBegin() {
  if (sReady && sCount == sConfiguredCount && sConfiguredCount > 0) return true;
  if (sConfiguredCount == 0 || sConfiguredCount > SHOWDUINO_PIXEL_NODE_MAX_PIXELS) {
    Serial.printf("[PIXEL] GPIO%d waiting for PIXEL:COUNT:<1-%u> then PIXEL:INIT\n",
                  SHOWDUINO_PIXEL_DATA_PIN,
                  (unsigned)SHOWDUINO_PIXEL_NODE_MAX_PIXELS);
    return false;
  }
  if (sReady) pixelEngineShutdown();

  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) {
    sSegments[i] = showduinoPixelDefaultSegment();
  }

  sCount = sConfiguredCount;
  sFrame = (uint8_t *)calloc((size_t)sCount * 3U, 1);
#if SHOWDUINO_PIXEL_USE_FASTLED
  sFastLeds = new (std::nothrow) CRGB[sCount];
  if (!sFrame || !sFastLeds) {
#else
  sStrip = new Adafruit_NeoPixel(sCount, SHOWDUINO_PIXEL_DATA_PIN,
                                 SHOWDUINO_PIXEL_ORDER);
  if (!sFrame || !sStrip) {
#endif
    Serial.println("[PIXEL] Show line allocation failed");
    pixelEngineShutdown();
    return false;
  }

  pinMode(SHOWDUINO_PIXEL_DATA_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_PIXEL_DATA_PIN, LOW);
#if SHOWDUINO_PIXEL_USE_FASTLED
  // Register once; rebinding prevents duplicate controller leaks on reinit.
  if (!sFastController) {
    sFastController = &FastLED.addLeds<WS2812, SHOWDUINO_PIXEL_DATA_PIN,
                                       SHOWDUINO_PIXEL_FASTLED_ORDER>(sFastLeds, sCount);
  } else {
    sFastController->setLeds(sFastLeds, sCount);
  }
  FastLED.setBrightness(255);
#else
  sStrip->begin();
  sStrip->clear();
#endif
  sReady = true;
  clearFrame();
  writeFrame();
  Serial.printf("[PIXEL] Line initialised GPIO=%d count=%u segments=%u resistor=%uR\n",
                SHOWDUINO_PIXEL_DATA_PIN,
                (unsigned)sCount,
                (unsigned)SHOWDUINO_PIXEL_MAX_SEGMENTS,
                (unsigned)SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS);
  return true;
}

void pixelEngineApplyPersisted() {
  sGlobalBrightness = nodeConfigGetU8("bri", 255);
  const uint16_t n = nodeConfigGetU16("pix", 0);
  if (n > 0 && n <= SHOWDUINO_PIXEL_NODE_MAX_PIXELS) sConfiguredCount = n;
  if (sConfiguredCount == 0) {
    Serial.printf("[PIXEL] GPIO%d not initialised — PIXEL:COUNT then PIXEL:INIT (1-%u)\n",
                  SHOWDUINO_PIXEL_DATA_PIN,
                  (unsigned)SHOWDUINO_PIXEL_NODE_MAX_PIXELS);
    return;
  }
  (void)pixelEngineBegin();
}

void pixelEngineSetConfiguredCount(uint16_t count) {
  sConfiguredCount = count;
}

void pixelEngineRelease() {
  pixelEngineShutdown();
  sConfiguredCount = 0;
  pixelEngineSafeGpio();
}

void pixelEngineBlackout() {
  sTestActive = false;
  sLocateActive = false;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) {
    sSegments[i].active = false;
    sSegments[i].effect = ShowduinoPixelFx::Off;
  }
  if (!sReady) return;
  clearFrame();
  writeFrame();
}

void pixelEngineOnEmergency(bool active) {
  sLocateActive = false;
  if (!sReady) {
    if (sConfiguredCount == 0) {
      sEmergency = active;
      return;
    }
    if (!pixelEngineBegin()) return;
  }
  sEmergency = active;
  sTestActive = false;
  if (active) {
    renderEmergencyWhite();
    writeFrame();
    sLastEmergencyRefreshMs = millis();
    Serial.println("[PIXEL] EMERGENCY → entire configured line bright white");
  } else {
    for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) sSegments[i].active = false;
    clearFrame();
    writeFrame();
    Serial.println("[PIXEL] Emergency cleared → line remains OFF until P4 resync");
  }
}

void pixelEngineStartLocate() {
  if (!sReady || sEmergency) return;
  sLocateActive = true;
  sTestActive = false;
  sLocateStartedMs = millis();
  Serial.println("[PIXEL] LOCATE");
}

static void renderLocate(uint32_t now) {
  const uint32_t elapsed = now - sLocateStartedMs;
  if (elapsed >= SHOWDUINO_PIXEL_LOCATE_MS) {
    sLocateActive = false;
    pixelEngineBlackout();
    return;
  }
  clearFrame();
  const uint16_t pos = (uint16_t)((elapsed / 40UL) % (sCount ? sCount : 1));
  const uint8_t on = ((elapsed / 120UL) % 2U) ? 255 : 70;
  if (sCount) setPixelRaw(pos, on, on, on);
  if (sCount > 1) setPixelRaw((uint16_t)((pos + sCount / 2U) % sCount), on, on, on);
}

void pixelEngineService() {
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

  if ((now - sLastFrameMs) < SHOWDUINO_PIXEL_FRAME_MS) return;
  sLastFrameMs = now;

  if (sLocateActive) {
    renderLocate(now);
    writeFrame();
    return;
  }

  if (sTestActive) {
    renderCommissioningTest(now);
    writeFrame();
    return;
  }

  clearFrame();
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) renderSegment(sSegments[i], now);
  writeFrame();
}

bool pixelEngineReady() { return sReady; }
bool pixelEngineEmergency() { return sEmergency; }
bool pixelEngineLocateActive() { return sLocateActive; }
uint16_t pixelEngineCount() { return sCount; }
uint16_t pixelEngineConfiguredCount() { return sConfiguredCount; }
uint16_t pixelEngineMax() { return SHOWDUINO_PIXEL_NODE_MAX_PIXELS; }
uint8_t pixelEngineGlobalBrightness() { return sGlobalBrightness; }
int pixelEnginePin() { return SHOWDUINO_PIXEL_DATA_PIN; }

uint8_t pixelEngineActiveSegments() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) {
    if (sSegments[i].configured && sSegments[i].count) n++;
  }
  return n;
}

const char *pixelEngineBackendName() {
#if SHOWDUINO_PIXEL_USE_FASTLED
  return "FastLED";
#else
  return "Adafruit_NeoPixel";
#endif
}

void pixelEnginePrintStatus() {
  Serial.printf("[PIXEL] Backend=%s\n", pixelEngineBackendName());
  Serial.printf("[PIXEL] GPIO=%d configured=%u active=%u ready=%s brightness=%u emergency=%s locate=%s\n",
                SHOWDUINO_PIXEL_DATA_PIN,
                (unsigned)sConfiguredCount,
                (unsigned)sCount,
                sReady ? "YES" : "NO",
                (unsigned)sGlobalBrightness,
                sEmergency ? "WHITE_OVERRIDE" : "CLEAR",
                sLocateActive ? "YES" : "NO");
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_MAX_SEGMENTS; ++i) {
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

bool pixelEngineHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (reply && replyLen) reply[0] = '\0';
  if (!command || strncmp(command, "PIXEL:", 6) != 0) return false;

  String cmd(command);
  String p[12];
  uint8_t n = splitTokens(cmd, p, 12);

  if (cmd == "PIXEL:STATUS") {
    pixelEnginePrintStatus();
    char line[160];
    snprintf(line, sizeof(line),
             "PIXEL:STATUS:%s:GPIO=%d:CONFIGURED=%u:COUNT=%u:MAX=%u:BRIGHTNESS=%u:EMERGENCY=%u",
             sReady ? "READY" : "UNINIT", SHOWDUINO_PIXEL_DATA_PIN,
             (unsigned)sConfiguredCount, (unsigned)sCount,
             (unsigned)SHOWDUINO_PIXEL_NODE_MAX_PIXELS,
             (unsigned)sGlobalBrightness, sEmergency ? 1U : 0U);
    setReply(reply, replyLen, line);
    return true;
  }

  if (n == 3 && p[1] == "COUNT") {
    if (sEmergency) {
      setReply(reply, replyLen, "PIXEL:REJECTED:EMERGENCY_ACTIVE");
      return true;
    }
    uint16_t count = 0;
    if (!parseU16(p[2], &count) || !showduino_pixel_count_ok(count, SHOWDUINO_PIXEL_NODE_MAX_PIXELS)) {
      setReply(reply, replyLen, "PIXEL:ERROR:COUNT");
      return true;
    }
    sConfiguredCount = count;
    nodeConfigSetU16("pix", count);
    char line[80];
    snprintf(line, sizeof(line), "PIXEL:COUNT:OK:%u", (unsigned)count);
    setReply(reply, replyLen, line);
    Serial.printf("[PIXEL] Line length set to %u — PIXEL:INIT to apply\n", (unsigned)count);
    return true;
  }

  if (cmd == "PIXEL:INIT") {
    if (sEmergency) {
      setReply(reply, replyLen, "PIXEL:REJECTED:EMERGENCY_ACTIVE");
      return true;
    }
    if (pixelEngineBegin()) {
      char line[80];
      snprintf(line, sizeof(line), "PIXEL:INIT:OK:%u", (unsigned)sCount);
      setReply(reply, replyLen, line);
    } else {
      setReply(reply, replyLen, "PIXEL:ERROR:INIT");
    }
    return true;
  }

  if (!sReady) {
    setReply(reply, replyLen, "PIXEL:ERROR:NOT_INITIALISED");
    return true;
  }

  if (sEmergency) {
    setReply(reply, replyLen, "PIXEL:REJECTED:EMERGENCY_ACTIVE");
    return true;
  }

  if (cmd == "PIXEL:OFF" || cmd == "PIXEL:BLACKOUT") {
    pixelEngineBlackout();
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
    pixelEngineBlackout();
    setReply(reply, replyLen, "PIXEL:TEST:STOPPED");
    return true;
  }
  if (cmd == "PIXEL:LOCATE") {
    pixelEngineStartLocate();
    setReply(reply, replyLen, "PIXEL:LOCATE:OK");
    return true;
  }

  if (n == 3 && p[1] == "BRIGHTNESS") {
    uint8_t value;
    if (!parseU8(p[2], &value)) setReply(reply, replyLen, "PIXEL:ERROR:BRIGHTNESS");
    else {
      sGlobalBrightness = value;
      nodeConfigSetU8("bri", value);
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
    seg.configured = true; seg.active = true; seg.start = 0; seg.count = sCount;
    seg.effect = ShowduinoPixelFx::Solid; seg.primary = {r, g, b}; seg.startedMs = millis();
    setReply(reply, replyLen, "PIXEL:SOLID:OK");
    return true;
  }

  if (n < 4 || p[1] != "SEGMENT") {
    setReply(reply, replyLen, "PIXEL:ERROR:UNKNOWN_COMMAND");
    return true;
  }

  uint8_t segId;
  if (!parseU8(p[2], &segId, SHOWDUINO_PIXEL_MAX_SEGMENTS - 1U)) {
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
        start >= sCount || (uint32_t)start + count > sCount) {
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
