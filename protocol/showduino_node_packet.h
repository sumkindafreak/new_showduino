#ifndef SHOWDUINO_NODE_PACKET_H
#define SHOWDUINO_NODE_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "showduino_protocol_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Communications Engine <-> Node ESP-NOW packet (protocol v1 wire).
 * No magic field on the wire today — size-based discrimination on C3.
 * Expected sizeof == 116.
 */
typedef struct ShowduinoNodePacket {
  char nodeType[SHOWDUINO_NODE_TYPE_MAX];
  char command[SHOWDUINO_NODE_COMMAND_MAX];
  uint32_t sequence;
} ShowduinoNodePacket;

static inline void showduino_node_packet_clear(ShowduinoNodePacket *p) {
  if (p) {
    memset(p, 0, sizeof(*p));
  }
}

static inline void showduino_node_packet_init(
    ShowduinoNodePacket *p,
    const char *nodeType,
    uint32_t sequence) {
  size_t n;
  showduino_node_packet_clear(p);
  if (!p) return;
  p->sequence = sequence;
  if (!nodeType) return;
  n = strlen(nodeType);
  if (n >= SHOWDUINO_NODE_TYPE_MAX) n = SHOWDUINO_NODE_TYPE_MAX - 1;
  memcpy(p->nodeType, nodeType, n);
  p->nodeType[n] = '\0';
}

static inline int showduino_node_set_command(ShowduinoNodePacket *p, const char *cmd) {
  size_t n;
  if (!p) return -1;
  if (!cmd) return -2;
  n = strlen(cmd);
  if (n == 0) return -2;
  if (n >= SHOWDUINO_NODE_COMMAND_MAX) return -3;
  memcpy(p->command, cmd, n);
  p->command[n] = '\0';
  return 0;
}

#ifdef __cplusplus
} /* extern "C" */
static_assert(sizeof(ShowduinoNodePacket) == SHOWDUINO_NODE_PACKET_SIZE_EXPECTED,
              "ShowduinoNodePacket wire size must remain 116");
#endif

#ifndef __cplusplus
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(ShowduinoNodePacket) == SHOWDUINO_NODE_PACKET_SIZE_EXPECTED,
               "ShowduinoNodePacket wire size must remain 116");
#endif
#endif

#endif /* SHOWDUINO_NODE_PACKET_H */
