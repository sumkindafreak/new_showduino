#ifndef SHOWDUINO_AUDIO_VOLUME_H
#define SHOWDUINO_AUDIO_VOLUME_H

#include <math.h>
#include <stdint.h>

// Percentage describes signal amplitude, not a percentage of the codec's
// 96 dB attenuation range. ES8388 attenuation uses positive half-dB steps.
static inline uint8_t showduinoAudioDacVolume(uint8_t percent) {
  if (percent == 0) return 0xC0;  // ES8388 minimum (-96 dB).
  if (percent >= 100) return 0;  // Unity gain, never boost.
  return (uint8_t)lroundf(-40.0f * log10f((float)percent / 100.0f));
}

#endif
