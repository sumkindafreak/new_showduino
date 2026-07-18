#ifndef SHOWDUINO_DESK_PACKET_H
#define SHOWDUINO_DESK_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "showduino_protocol_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Director <-> Communications Engine ESP-NOW desk packet (protocol v1 wire).
 * Natural alignment; do not pack. Expected sizeof == 108 on ESP32 / typical hosts.
 */
typedef struct ShowduinoDeskPacket {
  uint32_t magic;
  uint16_t version;
  uint16_t sequence;
  uint32_t sentMillis;
  char command[SHOWDUINO_DESK_COMMAND_MAX];
} ShowduinoDeskPacket;

/* Historical Director name — same layout. */
typedef ShowduinoDeskPacket ShowduinoEspNowPacket;

static inline void showduino_desk_packet_clear(ShowduinoDeskPacket *p) {
  if (p) {
    memset(p, 0, sizeof(*p));
  }
}

static inline void showduino_desk_packet_init(
    ShowduinoDeskPacket *p,
    uint16_t sequence,
    uint32_t sentMillis) {
  showduino_desk_packet_clear(p);
  if (!p) return;
  p->magic = SHOWDUINO_ESPNOW_MAGIC;
  p->version = SHOWDUINO_DESK_WIRE_VERSION;
  p->sequence = sequence;
  p->sentMillis = sentMillis;
}

/*
 * Copies NUL-terminated text into command[].
 * Returns 0 on success, -1 if dst is null, -2 if src null/empty, -3 if too long.
 */
static inline int showduino_desk_set_command(ShowduinoDeskPacket *p, const char *cmd) {
  size_t n;
  if (!p) return -1;
  if (!cmd) return -2;
  n = strlen(cmd);
  if (n == 0) return -2;
  if (n >= SHOWDUINO_DESK_COMMAND_MAX) return -3;
  memcpy(p->command, cmd, n);
  p->command[n] = '\0';
  return 0;
}

#ifdef __cplusplus
} /* extern "C" */
static_assert(sizeof(ShowduinoDeskPacket) == SHOWDUINO_DESK_PACKET_SIZE_EXPECTED,
              "ShowduinoDeskPacket wire size must remain 108");
#endif

#ifndef __cplusplus
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(ShowduinoDeskPacket) == SHOWDUINO_DESK_PACKET_SIZE_EXPECTED,
               "ShowduinoDeskPacket wire size must remain 108");
#endif
#endif

#endif /* SHOWDUINO_DESK_PACKET_H */
