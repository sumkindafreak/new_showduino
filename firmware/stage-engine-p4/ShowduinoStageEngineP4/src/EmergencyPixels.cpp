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
static void fillFrame(uint8_t g, uint8_t r, uint8_t b) {
  if (!sItems) return;
  size_t i = 0;
  for (uint16_t led = 0; led < SHOWDUINO_EMERGENCY_PIXEL_COUNT; led++) {
    const uint8_t bytes[3] = {g, r, b}; /* WS2812 GRB */
    for (uint8_t bi = 0; bi < 3; bi++) {
      uint8_t v = bytes[bi];
      for (int bit = 7; bit >= 0; bit--) {
        if (v & (1 << bit)) {
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
  }
  sSymbolCount = i;
}
#endif

static bool writeFrame(bool white) {
#if !SOC_RMT_SUPPORTED
  (void)white;
  return false;
#else
  if (!sReady || !sItems) return false;
  fillFrame(white ? 255 : 0, white ? 255 : 0, white ? 255 : 0);
  /* Timed write — never WAIT_FOR_EVER. A hang here used to stall setup()
   * before GPIO25 was configured, so the physical button never ran. */
  const bool ok = rmtWrite(SHOWDUINO_EMERGENCY_PIXEL_PIN, sItems, sSymbolCount, 80);
  if (!ok) {
    Serial.println("[PIXEL] RMT write failed or timed out");
  }
  return ok;
#endif
}

bool emergencyPixelsBegin() {
  if (sReady) return true;

#if !SOC_RMT_SUPPORTED
  Serial.println("[PIXEL] RMT not supported on this chip — emergency strip disabled");
  return false;
#else
  pinMode(SHOWDUINO_EMERGENCY_PIXEL_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_EMERGENCY_PIXEL_PIN, LOW);

  const size_t bits = (size_t)SHOWDUINO_EMERGENCY_PIXEL_COUNT * 24U;
  sItems = (rmt_data_t *)malloc(bits * sizeof(rmt_data_t));
  if (!sItems) {
    Serial.println("[PIXEL] Emergency strip alloc failed");
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
  Serial.printf("[PIXEL] Emergency NeoPixel GPIO=%d count=%u RMT ready\n",
                SHOWDUINO_EMERGENCY_PIXEL_PIN,
                (unsigned)SHOWDUINO_EMERGENCY_PIXEL_COUNT);
  writeFrame(false);
  return true;
#endif
}

void emergencyPixelsSetWhite() {
  if (!sReady && !emergencyPixelsBegin()) return;
  sWhiteActive = true;
  sLastRefreshMs = millis();
  if (writeFrame(true)) {
    Serial.println("[PIXEL] EMERGENCY → white");
  }
}

void emergencyPixelsBlackout() {
  sWhiteActive = false;
  if (!sReady) return;
  if (writeFrame(false)) {
    Serial.println("[PIXEL] EMERGENCY clear → blackout");
  }
}

void emergencyPixelsService() {
  if (!sWhiteActive || !sReady) return;
  const uint32_t now = millis();
  if ((now - sLastRefreshMs) < 250UL) return;
  sLastRefreshMs = now;
  writeFrame(true);
}

bool emergencyPixelsReady() {
  return sReady;
}

bool emergencyPixelsWhiteActive() {
  return sWhiteActive;
}

bool emergencyPixelsWriteRgb(uint8_t r, uint8_t g, uint8_t b) {
#if !SOC_RMT_SUPPORTED
  (void)r;
  (void)g;
  (void)b;
  return false;
#else
  if (!sReady && !emergencyPixelsBegin()) return false;
  if (!sItems) return false;
  fillFrame(g, r, b);
  const bool ok = rmtWrite(SHOWDUINO_EMERGENCY_PIXEL_PIN, sItems, sSymbolCount, 80);
  if (!ok) {
    Serial.println("[PIXEL] RMT write failed or timed out");
  }
  return ok;
#endif
}

#endif
