#include "EmergencyPixels.h"

#if SHOWDUINO_EMERGENCY_PIXEL_ENABLED

#include "soc/soc_caps.h"
#if SOC_RMT_SUPPORTED
#include "esp32-hal-rmt.h"
#endif

static bool sReady = false;
static bool sWhiteActive = false;
static uint32_t sLastRefreshMs = 0;
#if SOC_RMT_SUPPORTED
static rmt_data_t *sItems = nullptr;
#endif
static size_t sSymbolCount = 0;

#if SOC_RMT_SUPPORTED
static void appendByte(size_t &i, uint8_t v) {
  for (int bit = 7; bit >= 0; bit--) {
    if (v & (1U << bit)) {
      sItems[i].level0 = 1;
      sItems[i].duration0 = 8; /* 0.80 us at 10 MHz */
      sItems[i].level1 = 0;
      sItems[i].duration1 = 4; /* 0.40 us */
    } else {
      sItems[i].level0 = 1;
      sItems[i].duration0 = 4; /* 0.40 us */
      sItems[i].level1 = 0;
      sItems[i].duration1 = 8; /* 0.80 us */
    }
    i++;
  }
}

static void fillUniformFrame(uint8_t r, uint8_t g, uint8_t b) {
  if (!sItems) return;
  size_t i = 0;
  for (uint16_t led = 0; led < SHOWDUINO_EMERGENCY_PIXEL_COUNT; led++) {
    appendByte(i, g); /* WS2812 GRB */
    appendByte(i, r);
    appendByte(i, b);
  }
  sSymbolCount = i;
}

static void fillNormalSignFrame() {
  if (!sItems) return;
  size_t i = 0;
  for (uint16_t led = 0; led < SHOWDUINO_EMERGENCY_PIXEL_COUNT; led++) {
    const uint8_t position = (uint8_t)(led % SHOWDUINO_EMERGENCY_SIGN_PIXELS);
    const bool locator = position == SHOWDUINO_EMERGENCY_SIGN_LOCATOR_INDEX;
    const uint8_t r = locator ? SHOWDUINO_EMERGENCY_SIGN_NORMAL_R : 0;
    const uint8_t g = locator ? SHOWDUINO_EMERGENCY_SIGN_NORMAL_G : 0;
    const uint8_t b = locator ? SHOWDUINO_EMERGENCY_SIGN_NORMAL_B : 0;
    appendByte(i, g);
    appendByte(i, r);
    appendByte(i, b);
  }
  sSymbolCount = i;
}
#endif

static bool transmitPreparedFrame() {
#if !SOC_RMT_SUPPORTED
  return false;
#else
  if (!sReady || !sItems) return false;
  const bool ok = rmtWrite(SHOWDUINO_EMERGENCY_PIXEL_PIN, sItems, sSymbolCount, 80);
  if (!ok) Serial.println("[PIXEL] Emergency line RMT write failed or timed out");
  return ok;
#endif
}

static bool writeUniform(uint8_t r, uint8_t g, uint8_t b) {
#if !SOC_RMT_SUPPORTED
  (void)r; (void)g; (void)b;
  return false;
#else
  if (!sReady || !sItems) return false;
  fillUniformFrame(r, g, b);
  return transmitPreparedFrame();
#endif
}

static bool writeNormal() {
#if !SOC_RMT_SUPPORTED
  return false;
#else
  if (!sReady || !sItems) return false;
  fillNormalSignFrame();
  return transmitPreparedFrame();
#endif
}

bool emergencyPixelsBegin() {
  if (sReady) return true;
#if !SOC_RMT_SUPPORTED
  Serial.println("[PIXEL] RMT not supported on this chip — emergency/signage line disabled");
  return false;
#else
  pinMode(SHOWDUINO_EMERGENCY_PIXEL_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_EMERGENCY_PIXEL_PIN, LOW);

  const size_t bits = (size_t)SHOWDUINO_EMERGENCY_PIXEL_COUNT * 24U;
  sItems = (rmt_data_t *)malloc(bits * sizeof(rmt_data_t));
  if (!sItems) {
    Serial.println("[PIXEL] Emergency/signage line alloc failed");
    return false;
  }

  bool inited = rmtInit(SHOWDUINO_EMERGENCY_PIXEL_PIN, RMT_TX_MODE,
                        RMT_MEM_NUM_BLOCKS_2, 10000000);
  if (!inited) {
    inited = rmtInit(SHOWDUINO_EMERGENCY_PIXEL_PIN, RMT_TX_MODE,
                     RMT_MEM_NUM_BLOCKS_1, 10000000);
  }
  if (!inited) {
    Serial.printf("[PIXEL] RMT init failed on GPIO %d\n", SHOWDUINO_EMERGENCY_PIXEL_PIN);
    free(sItems);
    sItems = nullptr;
    return false;
  }

  rmtSetEOT(SHOWDUINO_EMERGENCY_PIXEL_PIN, 0);
  sReady = true;
  if ((SHOWDUINO_EMERGENCY_PIXEL_COUNT % SHOWDUINO_EMERGENCY_SIGN_PIXELS) != 0) {
    Serial.printf("[PIXEL] WARNING: emergency count %u is not a multiple of sign size %u\n",
                  (unsigned)SHOWDUINO_EMERGENCY_PIXEL_COUNT,
                  (unsigned)SHOWDUINO_EMERGENCY_SIGN_PIXELS);
  }
  Serial.printf("[PIXEL] Emergency/signage GPIO=%d count=%u sign-size=%u signs=%u RMT ready\n",
                SHOWDUINO_EMERGENCY_PIXEL_PIN,
                (unsigned)SHOWDUINO_EMERGENCY_PIXEL_COUNT,
                (unsigned)SHOWDUINO_EMERGENCY_SIGN_PIXELS,
                (unsigned)((SHOWDUINO_EMERGENCY_PIXEL_COUNT + SHOWDUINO_EMERGENCY_SIGN_PIXELS - 1U) /
                           SHOWDUINO_EMERGENCY_SIGN_PIXELS));
  emergencyPixelsSetNormal();
  return true;
#endif
}

void emergencyPixelsSetNormal() {
  if (!sReady && !emergencyPixelsBegin()) return;
  sWhiteActive = false;
  if (writeNormal()) {
    Serial.println("[PIXEL] Emergency signage NORMAL → first pixel of each 10-pixel sign GREEN");
  }
}

void emergencyPixelsSetWhite() {
  if (!sReady && !emergencyPixelsBegin()) return;
  sWhiteActive = true;
  sLastRefreshMs = millis();
  const uint8_t white = SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS;
  if (writeUniform(white, white, white)) {
    Serial.println("[PIXEL] EMERGENCY → ALL emergency/signage pixels WHITE");
  }
}

void emergencyPixelsBlackout() {
  sWhiteActive = false;
  if (!sReady) return;
  if (writeUniform(0, 0, 0)) Serial.println("[PIXEL] Emergency/signage diagnostic blackout");
}

void emergencyPixelsService() {
  if (!sWhiteActive || !sReady) return;
  const uint32_t now = millis();
  if ((now - sLastRefreshMs) < 250UL) return;
  sLastRefreshMs = now;
  const uint8_t white = SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS;
  writeUniform(white, white, white);
}

bool emergencyPixelsReady() { return sReady; }
bool emergencyPixelsWhiteActive() { return sWhiteActive; }

bool emergencyPixelsWriteRgb(uint8_t r, uint8_t g, uint8_t b) {
#if !SOC_RMT_SUPPORTED
  (void)r; (void)g; (void)b;
  return false;
#else
  if (!sReady && !emergencyPixelsBegin()) return false;
  return writeUniform(r, g, b);
#endif
}

#endif
