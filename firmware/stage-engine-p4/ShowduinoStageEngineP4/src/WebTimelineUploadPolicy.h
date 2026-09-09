#ifndef SHOWDUINO_WEB_TIMELINE_UPLOAD_POLICY_H
#define SHOWDUINO_WEB_TIMELINE_UPLOAD_POLICY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Browser/Studio RAM-timeline upload policy.
 *
 * This is deliberately narrower than the P4's normal command surface. A
 * SHOW:TL:C envelope from Studio may carry only show-output commands that are
 * already implemented and separately validated by the P4:
 *
 *   - PIXEL:*       -> P4-local GPIO23 Show Pixel Engine
 *   - AUDIO:NODE:*  -> specialist Audio Node programme audio
 *
 * Emergency, production, network, storage, system-audio, SHOW:* and arbitrary
 * commands are never valid as a nested browser timeline payload. The P4
 * remains authoritative and still applies its normal command/emergency gates
 * when the cue later fires.
 */

namespace ShowduinoWebTimelineUploadPolicy {

static constexpr size_t kTimelineCommandMax = 63;  // TimelineEngine payload limit.

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

  /* uint32_t maximum is 4294967295; reject longer/evidently overflowing text
     before the normal ShowRuntimeOwner parser converts the timestamp. */
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
