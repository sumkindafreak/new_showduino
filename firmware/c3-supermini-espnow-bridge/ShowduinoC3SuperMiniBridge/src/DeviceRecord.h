#ifndef SHOWDUINO_DEVICE_RECORD_H
#define SHOWDUINO_DEVICE_RECORD_H

#include <Arduino.h>
#include <stdint.h>

enum class DevicePresence : uint8_t {
  Offline = 0,
  Warning = 1,
  Online = 2
};

enum class DeviceAvailability : uint8_t {
  Available = 0,
  Busy = 1,
  Unavailable = 2
};

struct DeviceRecord {
  bool inUse = false;
  char id[28] = {0};
  char boardType[28] = {0};
  char friendlyName[36] = {0};
  uint8_t mac[6] = {0};
  char firmwareVersion[20] = {0};
  char protocolVersion[12] = {0};
  DevicePresence presence = DevicePresence::Offline;
  DeviceAvailability availability = DeviceAvailability::Unavailable;
  uint32_t lastSeenMs = 0;
  uint32_t discoveredMs = 0;
  int8_t rssi = 0;
  char ip[16] = {0};
  bool wifiConnected = false;
  bool espNowActive = false;
  char capabilities[128] = {0};
  char batteryStatus[16] = "n/a";
  char connectionType[20] = {0};
  uint8_t routePriority = 50;
  bool preferred = false;
};

static inline const char *devicePresenceName(DevicePresence p) {
  switch (p) {
    case DevicePresence::Online: return "online";
    case DevicePresence::Warning: return "warning";
    default: return "offline";
  }
}

static inline const char *deviceAvailabilityName(DeviceAvailability a) {
  switch (a) {
    case DeviceAvailability::Available: return "available";
    case DeviceAvailability::Busy: return "busy";
    default: return "unavailable";
  }
}

static inline void deviceMacToString(const uint8_t mac[6], char *out, size_t outLen) {
  if (!out || outLen < 18) return;
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline void deviceIdFromMac(const char *boardType, const uint8_t mac[6], char *out, size_t outLen) {
  if (!out || outLen < 20) return;
  snprintf(out, outLen, "%s-%02X%02X%02X%02X%02X%02X",
           boardType && boardType[0] ? boardType : "node",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#endif