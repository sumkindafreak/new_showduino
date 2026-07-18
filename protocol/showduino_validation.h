#ifndef SHOWDUINO_VALIDATION_H
#define SHOWDUINO_VALIDATION_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "showduino_protocol_version.h"
#include "showduino_desk_packet.h"
#include "showduino_node_packet.h"
#include "showduino_message_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ShowduinoValidateResult {
  SHOWDUINO_VALID = 0,
  SHOWDUINO_INVALID_MAGIC,
  SHOWDUINO_UNSUPPORTED_VERSION,
  SHOWDUINO_INVALID_SIZE,
  SHOWDUINO_PAYLOAD_TOO_LONG,
  SHOWDUINO_PAYLOAD_NOT_TERMINATED,
  SHOWDUINO_EMPTY_PAYLOAD,
  SHOWDUINO_INVALID_MESSAGE,
  SHOWDUINO_INVALID_RELAY_CHANNEL,
  SHOWDUINO_INVALID_RELAY_STATE,
  SHOWDUINO_INVALID_NODE_TYPE
} ShowduinoValidateResult;

static inline int showduino_payload_is_terminated(const char *buf, size_t capacity) {
  size_t i;
  if (!buf || capacity == 0) return 0;
  for (i = 0; i < capacity; i++) {
    if (buf[i] == '\0') return 1;
  }
  return 0;
}

static inline ShowduinoValidateResult showduino_validate_desk_rx(
    const void *data,
    size_t length) {
  const ShowduinoDeskPacket *p;
  if (!data) return SHOWDUINO_INVALID_SIZE;
  if (length != sizeof(ShowduinoDeskPacket)) return SHOWDUINO_INVALID_SIZE;
  p = (const ShowduinoDeskPacket *)data;
  if (p->magic != SHOWDUINO_ESPNOW_MAGIC) return SHOWDUINO_INVALID_MAGIC;
  /* v1: wire version field must equal major/wire version 1 */
  if (p->version != SHOWDUINO_DESK_WIRE_VERSION) return SHOWDUINO_UNSUPPORTED_VERSION;
  if (!showduino_payload_is_terminated(p->command, SHOWDUINO_DESK_COMMAND_MAX)) {
    return SHOWDUINO_PAYLOAD_NOT_TERMINATED;
  }
  if (p->command[0] == '\0') return SHOWDUINO_EMPTY_PAYLOAD;
  return SHOWDUINO_VALID;
}

static inline ShowduinoValidateResult showduino_validate_node_rx(
    const void *data,
    size_t length) {
  const ShowduinoNodePacket *p;
  if (!data) return SHOWDUINO_INVALID_SIZE;
  if (length != sizeof(ShowduinoNodePacket)) return SHOWDUINO_INVALID_SIZE;
  p = (const ShowduinoNodePacket *)data;
  if (!showduino_payload_is_terminated(p->nodeType, SHOWDUINO_NODE_TYPE_MAX)) {
    return SHOWDUINO_PAYLOAD_NOT_TERMINATED;
  }
  if (!showduino_payload_is_terminated(p->command, SHOWDUINO_NODE_COMMAND_MAX)) {
    return SHOWDUINO_PAYLOAD_NOT_TERMINATED;
  }
  if (p->nodeType[0] == '\0') return SHOWDUINO_INVALID_NODE_TYPE;
  if (p->command[0] == '\0') return SHOWDUINO_EMPTY_PAYLOAD;
  return SHOWDUINO_VALID;
}

static inline ShowduinoValidateResult showduino_validate_relay_channel(int channel) {
  if (channel < 1 || channel > 8) return SHOWDUINO_INVALID_RELAY_CHANNEL;
  return SHOWDUINO_VALID;
}

static inline ShowduinoValidateResult showduino_validate_relay_state(int state) {
  if (state != SHOWDUINO_RELAY_OFF && state != SHOWDUINO_RELAY_ON) {
    return SHOWDUINO_INVALID_RELAY_STATE;
  }
  return SHOWDUINO_VALID;
}

/*
 * Safe copy into fixed buffer. Returns SHOWDUINO_VALID or error.
 * Never truncates silently.
 */
static inline ShowduinoValidateResult showduino_safe_copy_command(
    char *dst,
    size_t dstCapacity,
    const char *src) {
  size_t n;
  if (!dst || dstCapacity == 0) return SHOWDUINO_INVALID_SIZE;
  if (!src) return SHOWDUINO_EMPTY_PAYLOAD;
  n = strlen(src);
  if (n == 0) return SHOWDUINO_EMPTY_PAYLOAD;
  if (n >= dstCapacity) return SHOWDUINO_PAYLOAD_TOO_LONG;
  memcpy(dst, src, n);
  dst[n] = '\0';
  return SHOWDUINO_VALID;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_VALIDATION_H */
