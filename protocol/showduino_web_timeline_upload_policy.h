#ifndef SHOWDUINO_WEB_TIMELINE_UPLOAD_POLICY_H
#define SHOWDUINO_WEB_TIMELINE_UPLOAD_POLICY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Narrow browser/Studio RAM-timeline upload envelope.
 *
 * This policy is intentionally much smaller than the normal command surface.
 * Studio may upload only:
 *
 *   SHOW:TL:BEGIN
 *   SHOW:TL:C:<timeMs>:PIXEL:...
 *   SHOW:TL:C:<timeMs>:AUDIO:NODE:...
 *   SHOW:TL:END
 *
 * The P4 still applies its normal detailed PIXEL/AUDIO:NODE validation and its
 * emergency/runtime gates. No emergency, SHOW, production, network, storage,
 * system-audio, Relay, DMX or arbitrary command may be nested in this envelope.
 */
namespace ShowduinoWebTimelineUploadPolicy {

static constexpr size_t kTimelineCommandMax = 63;

inline bool isDigits(const char *text, size_t len) {
  if (!text || len == 0) return false;
  for (size_t i = 0; i < len; ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
  }
  return true;
}

inline bool payloadFamilyAllowed(const char *payload) {
  if (!payload || !payload[0]) return false;
  return strncmp(payload, "PIXEL:", 6) == 0 ||
         strncmp(payload, "AUDIO:NODE:", 11) == 0;
}

inline bool cueEnvelopeAllowed(const char *command,
                               const char **payloadOut = nullptr) {
  static const char kPrefix[] = "SHOW:TL:C:";
  if (!command || strncmp(command, kPrefix, sizeof(kPrefix) - 1) != 0) {
    return false;
  }

  const char *timeStart = command + sizeof(kPrefix) - 1;
  const char *separator = strchr(timeStart, ':');
  if (!separator || separator == timeStart) return false;

  const size_t timeLen = (size_t)(separator - timeStart);
  if (timeLen > 10 || !isDigits(timeStart, timeLen)) return false;

  uint64_t timeValue = 0;
  for (size_t i = 0; i < timeLen; ++i) {
    timeValue = timeValue * 10u + (uint64_t)(timeStart[i] - '0');
    if (timeValue > 0xFFFFFFFFULL) return false;
  }

  const char *payload = separator + 1;
  const size_t payloadLen = strlen(payload);
  if (payloadLen == 0 || payloadLen > kTimelineCommandMax) return false;
  if (strchr(payload, '\r') || strchr(payload, '\n')) return false;
  if (!payloadFamilyAllowed(payload)) return false;

  if (payloadOut) *payloadOut = payload;
  return true;
}

inline bool envelopeAllowed(const char *command,
                            const char **payloadOut = nullptr) {
  if (!command) return false;
  if (strcmp(command, "SHOW:TL:BEGIN") == 0 ||
      strcmp(command, "SHOW:TL:END") == 0) {
    if (payloadOut) *payloadOut = nullptr;
    return true;
  }
  return cueEnvelopeAllowed(command, payloadOut);
}

}  // namespace ShowduinoWebTimelineUploadPolicy

#endif
