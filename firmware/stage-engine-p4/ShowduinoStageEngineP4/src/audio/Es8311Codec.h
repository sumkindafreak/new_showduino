#ifndef SHOWDUINO_ES8311_CODEC_H
#define SHOWDUINO_ES8311_CODEC_H

#include <Arduino.h>

/*
 * Onboard Waveshare ESP32-P4-Module-DEV-KIT ES8311.
 * I²C address 0x18 on the Plug-in Bus (GPIO7 SDA / GPIO8 SCL).
 * Playback path only: P4 I2S master → ES8311 DAC → NS4150B → speaker jack.
 * Attraction/programme audio is not this driver's job.
 */

#ifndef P4_ES8311_I2C_ADDR
#define P4_ES8311_I2C_ADDR  0x18U
#endif

enum class Es8311Status : uint8_t {
  Unknown = 0,
  Missing,
  Detected,
  Ready,
  Fault
};

struct Es8311Info {
  Es8311Status status = Es8311Status::Unknown;
  bool detected = false;
  bool initialized = false;
  bool muted = true;
  uint8_t chipId1 = 0;
  uint8_t chipId2 = 0;
  uint32_t sampleRate = 0;
  uint8_t volume = 0;
  char lastError[40] = "codec not started";
};

bool es8311Detect();
bool es8311Begin(uint32_t sampleRate, uint8_t volumePercent);
bool es8311SetSampleRate(uint32_t sampleRate);
bool es8311SetVolume(uint8_t volumePercent);
bool es8311SetMute(bool mute);
void es8311Standby();
const Es8311Info &es8311Info();
const char *es8311StatusName(Es8311Status s);

#endif
