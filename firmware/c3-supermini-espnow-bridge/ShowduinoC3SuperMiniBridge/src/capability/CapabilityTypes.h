#ifndef SHOWDUINO_CAPABILITY_TYPES_H
#define SHOWDUINO_CAPABILITY_TYPES_H

#include <stdint.h>

/** Canonical capability ids — independent of board type. Extend by appending. */
enum class CapabilityId : uint8_t {
  RelayOutput = 0,
  Lighting,
  AudioPlayback,
  GPIOInput,
  Temperature,
  Humidity,
  OLED,
  Touchscreen,
  DMXOutput,
  PixelOutput,
  Scheduler,
  SceneRuntime,
  MediaStorage,
  OTA,
  Logging,
  NetworkBridge,
  COUNT
};

const char *capabilityName(CapabilityId id);
bool capabilityFromName(const char *name, CapabilityId &out);
bool capabilityKnownName(const char *name);

#endif