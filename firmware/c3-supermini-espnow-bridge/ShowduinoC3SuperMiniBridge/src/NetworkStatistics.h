#ifndef SHOWDUINO_NETWORK_STATISTICS_H
#define SHOWDUINO_NETWORK_STATISTICS_H

#include <Arduino.h>
#include <stdint.h>

struct NetworkStatistics {
  uint16_t deviceCount = 0;
  uint16_t onlineCount = 0;
  uint16_t warningCount = 0;
  uint16_t offlineCount = 0;
  int16_t averageRssi = 0;
  uint32_t heartbeatRatePerMin = 0;
  char health[24] = "unknown";
  uint32_t computedMs = 0;
};

#endif
