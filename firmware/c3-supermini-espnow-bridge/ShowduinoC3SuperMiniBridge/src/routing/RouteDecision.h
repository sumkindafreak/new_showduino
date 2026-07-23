#ifndef SHOWDUINO_ROUTE_DECISION_H
#define SHOWDUINO_ROUTE_DECISION_H

#include <stdint.h>

struct RouteDecision {
  bool ok = false;
  char deviceId[28] = {0};
  char deviceName[36] = {0};
  char boardType[28] = {0};
  char decision[28] = {0};      /* broadcast|specific|best-match|preferred|fallback|unavailable */
  char capability[32] = {0};
  char path[96] = {0};          /* human-readable routing path */
  bool fallbackUsed = false;
  char reason[96] = {0};
  uint32_t resolvedMs = 0;
};

#endif