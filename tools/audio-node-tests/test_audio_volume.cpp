#include <cassert>
#include <cmath>
#include <cstdio>
#include "../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioVolume.h"

int main() {
  assert(showduinoAudioDacVolume(0) == 0xC0);
  assert(showduinoAudioDacVolume(100) == 0);
  assert(showduinoAudioDacVolume(255) == 0);
  assert(showduinoAudioDacVolume(50) == 12);  // -6 dB, formerly -48 dB.
  assert(showduinoAudioDacVolume(25) == 24);  // -12 dB.
  assert(showduinoAudioDacVolume(10) == 40);  // -20 dB.
  assert(showduinoAudioDacVolume(1) == 80);   // -40 dB.
  for (unsigned p = 1; p <= 100; ++p) {
    const unsigned attenuation = showduinoAudioDacVolume((uint8_t)p);
    assert(attenuation <= showduinoAudioDacVolume((uint8_t)(p - 1)));
    const double amplitude = std::pow(10.0, -(double)attenuation / 40.0);
    const double requested = (double)p / 100.0;
    // Nearest half-dB quantisation stays within a quarter dB of requested gain.
    assert(std::abs(20.0 * std::log10(amplitude / requested)) <= 0.251);
  }
  std::puts("PASS: amplitude volume, mute endpoint, unity cap, monotonicity and half-dB quantisation");
}
