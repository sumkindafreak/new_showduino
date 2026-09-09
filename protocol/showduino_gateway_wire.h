#ifndef SHOWDUINO_GATEWAY_WIRE_H
#define SHOWDUINO_GATEWAY_WIRE_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "showduino_version.h"

/*
 * Comms-owned external connectivity, forwarded Director ← Comms ESP-NOW.
 * P4 is not the authority for home Wi-Fi, internet, or GitHub.
 * Loss of internet is never STATE:SHOW / connection-lost.
 *
 *   STATE:GATEWAY:AP=ON,STA=OFF,INET=OFF,CH=1
 *   STATE:UPDATE:NONE
 *   STATE:UPDATE:CURRENT
 *   STATE:UPDATE:OFFLINE
 *   STATE:UPDATE:AVAILABLE:1.0.0-rc.2
 */

#define SHOWDUINO_WIRE_STATE_GATEWAY_PREFIX "STATE:GATEWAY:"
#define SHOWDUINO_WIRE_STATE_UPDATE_PREFIX  "STATE:UPDATE:"

#define SHOWDUINO_UPDATE_NONE      "NONE"
#define SHOWDUINO_UPDATE_CURRENT   "CURRENT"
#define SHOWDUINO_UPDATE_OFFLINE   "OFFLINE"
#define SHOWDUINO_UPDATE_AVAILABLE "AVAILABLE"
#define SHOWDUINO_UPDATE_FAILED    "FAILED"

#ifdef __cplusplus
extern "C" {
#endif

static inline int showduino_gateway_format(char *out, size_t outLen,
                                           int apOn, int staOn, int inetOn,
                                           unsigned channel) {
  if (!out || outLen < 24) return 0;
  int n = snprintf(out, outLen, SHOWDUINO_WIRE_STATE_GATEWAY_PREFIX
                   "AP=%s,STA=%s,INET=%s,CH=%u",
                   apOn ? "ON" : "OFF",
                   staOn ? "ON" : "OFF",
                   inetOn ? "ON" : "OFF",
                   channel);
  if (n <= 0 || (size_t)n >= outLen) return 0;
  return 1;
}

static inline int showduino_update_format(char *out, size_t outLen,
                                          const char *status,
                                          const char *latest) {
  if (!out || outLen < 16 || !status) return 0;
  int n;
  if (latest && latest[0] && strcmp(status, SHOWDUINO_UPDATE_AVAILABLE) == 0) {
    n = snprintf(out, outLen, SHOWDUINO_WIRE_STATE_UPDATE_PREFIX "%s:%s",
                 status, latest);
  } else {
    n = snprintf(out, outLen, SHOWDUINO_WIRE_STATE_UPDATE_PREFIX "%s", status);
  }
  if (n <= 0 || (size_t)n >= outLen) return 0;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_GATEWAY_WIRE_H */
