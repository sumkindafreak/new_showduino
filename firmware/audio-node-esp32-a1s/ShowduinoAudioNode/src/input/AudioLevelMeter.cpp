#include "AudioLevelMeter.h"
#include "../../../protocol/showduino_sound_input.h"

static uint64_t sAcc = 0;
static uint32_t sN = 0;
static uint32_t sPeakRaw = 0;
static uint8_t sRms = 0;
static uint8_t sPeak = 0;

static uint32_t isqrt32(uint32_t x) {
  uint32_t r = 0;
  uint32_t bit = 1u << 30;
  while (bit > x) bit >>= 2;
  while (bit) {
    if (x >= r + bit) {
      x -= r + bit;
      r = (r >> 1) + bit;
    } else {
      r >>= 1;
    }
    bit >>= 2;
  }
  return r;
}

void audioLevelMeterReset() {
  sAcc = 0;
  sN = 0;
  sPeakRaw = 0;
  sRms = 0;
  sPeak = 0;
}

void audioLevelMeterAddFrames(const int16_t *stereo, size_t frames) {
  if (!stereo || frames == 0) return;
  for (size_t i = 0; i < frames; i++) {
    const int32_t l = stereo[i * 2];
    const int32_t r = stereo[i * 2 + 1];
    int32_t m = (l >= 0 ? l : -l);
    const int32_t ar = (r >= 0 ? r : -r);
    if (ar > m) m = ar;
    sAcc += (uint64_t)((uint32_t)m * (uint32_t)m);
    sN++;
    if ((uint32_t)m > sPeakRaw) sPeakRaw = (uint32_t)m;
  }
}

void audioLevelMeterEndBlock() {
  if (sN == 0) {
    sRms = 0;
    sPeak = 0;
    return;
  }
  const uint32_t meanSq = (uint32_t)(sAcc / sN);
  sRms = showduino_sound_level_from_rms(isqrt32(meanSq));
  sPeak = showduino_sound_level_from_peak(sPeakRaw);
  sAcc = 0;
  sN = 0;
  sPeakRaw = 0;
}

uint8_t audioLevelMeterRms() { return sRms; }
uint8_t audioLevelMeterPeak() { return sPeak; }
