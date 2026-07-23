#include "CapabilityTypes.h"
#include <string.h>
#include <ctype.h>

static bool ieq(const char *a, const char *b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = (char)tolower((unsigned char)*a++);
    char cb = (char)tolower((unsigned char)*b++);
    if (ca != cb) return false;
  }
  return *a == 0 && *b == 0;
}

const char *capabilityName(CapabilityId id) {
  switch (id) {
    case CapabilityId::RelayOutput: return "RelayOutput";
    case CapabilityId::Lighting: return "Lighting";
    case CapabilityId::AudioPlayback: return "AudioPlayback";
    case CapabilityId::GPIOInput: return "GPIOInput";
    case CapabilityId::Temperature: return "Temperature";
    case CapabilityId::Humidity: return "Humidity";
    case CapabilityId::OLED: return "OLED";
    case CapabilityId::Touchscreen: return "Touchscreen";
    case CapabilityId::DMXOutput: return "DMXOutput";
    case CapabilityId::PixelOutput: return "PixelOutput";
    case CapabilityId::Scheduler: return "Scheduler";
    case CapabilityId::SceneRuntime: return "SceneRuntime";
    case CapabilityId::MediaStorage: return "MediaStorage";
    case CapabilityId::OTA: return "OTA";
    case CapabilityId::Logging: return "Logging";
    case CapabilityId::NetworkBridge: return "NetworkBridge";
    default: return "Unknown";
  }
}

bool capabilityFromName(const char *name, CapabilityId &out) {
  if (!name || !name[0]) return false;
  for (uint8_t i = 0; i < (uint8_t)CapabilityId::COUNT; i++) {
    CapabilityId id = (CapabilityId)i;
    if (ieq(name, capabilityName(id))) {
      out = id;
      return true;
    }
  }
  return false;
}

bool capabilityKnownName(const char *name) {
  CapabilityId id;
  return capabilityFromName(name, id);
}