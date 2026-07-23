#ifndef SHOWDUINO_HEARTBEAT_MANAGER_H
#define SHOWDUINO_HEARTBEAT_MANAGER_H

#include <Arduino.h>
#include <stdint.h>
#include "DeviceRecord.h"

class HeartbeatManager {
 public:
  void configure(uint32_t onlineMs, uint32_t warningMs, uint32_t offlineMs) {
    if (onlineMs > 0) onlineMs_ = onlineMs;
    if (warningMs > onlineMs_) warningMs_ = warningMs;
    if (offlineMs > warningMs_) offlineMs_ = offlineMs;
  }

  uint32_t onlineMs() const { return onlineMs_; }
  uint32_t warningMs() const { return warningMs_; }
  uint32_t offlineMs() const { return offlineMs_; }

  DevicePresence evaluate(uint32_t lastSeenMs, uint32_t nowMs) const {
    if (lastSeenMs == 0) return DevicePresence::Offline;
    const uint32_t age = nowMs - lastSeenMs;
    if (age <= onlineMs_) return DevicePresence::Online;
    if (age <= warningMs_) return DevicePresence::Warning;
    return DevicePresence::Offline;
  }

 private:
  uint32_t onlineMs_ = 5000;
  uint32_t warningMs_ = 12000;
  uint32_t offlineMs_ = 20000;
};

#endif
