#ifndef SHOWDUINO_EMERGENCY_NODE_H
#define SHOWDUINO_EMERGENCY_NODE_H

/*
 * Host-testable Emergency Node identity, latch machine, and wire helpers.
 * No Arduino, GPIO, ESP-NOW, or WebUI side effects.
 *
 * Protocol 1.0 additive. There is no wireless emergency-clear command.
 * An Emergency Node may ASSERT. It must never CLEAR.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "showduino_legacy_strings.h"
#include "showduino_protocol_version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_EMERGENCY_NODE_TYPE       "EMERGENCY"
#define SHOWDUINO_EMERGENCY_NODE_NAME       "Emergency Node"
#define SHOWDUINO_EMERGENCY_PROTOCOL        "1.0"
#define SHOWDUINO_EMERGENCY_ID_MAX          12
#define SHOWDUINO_EMERGENCY_NAME_MAX        20
#define SHOWDUINO_EMERGENCY_NODE_MAX_NODES  8
#define SHOWDUINO_EMERGENCY_ID_DEFAULT      "ESTOP-01"
#define SHOWDUINO_EMERGENCY_NAME_DEFAULT    "ENTRANCE"
#define SHOWDUINO_EMERGENCY_COMMS_TIMEOUT_MS 8000u
#define SHOWDUINO_EMERGENCY_HEARTBEAT_MS    2000u
#define SHOWDUINO_EMERGENCY_SEARCH_MS       1500u
#define SHOWDUINO_EMERGENCY_BURST_MS        250u
#define SHOWDUINO_EMERGENCY_BURST_COUNT     6u
#define SHOWDUINO_EMERGENCY_DEBOUNCE_MS     20u
#define SHOWDUINO_EMERGENCY_CAPS \
  "MOMENTARY_BUTTON,LOCAL_LATCH,ASSERT_ONLY,ESPNOW,HEARTBEAT,SOFTAP,NO_CLEAR,ONE_AT_A_TIME,OLED"
#define SHOWDUINO_EMERGENCY_UPDATE_POLICY   "ONE_AT_A_TIME"

#define SHOWDUINO_ESTOP_ASSERT_PREFIX       "ESTOP:ASSERT:"
#define SHOWDUINO_ESTOP_ACK_LATCHED         "ESTOP:ACK:LATCHED"
#define SHOWDUINO_ESTOP_GLOBAL_OBSERVED     "ESTOP:GLOBAL:OBSERVED"
#define SHOWDUINO_ESTOP_STATUS              "ESTOP:STATUS"
#define SHOWDUINO_ESTOP_OWN_GRANT           "ESTOP:OWN:GRANT"
#define SHOWDUINO_ESTOP_REARM               "ESTOP:REARM"

typedef enum ShowduinoEmergencyNodeState {
  SHOWDUINO_ESTOP_ST_UNKNOWN = 0,
  SHOWDUINO_ESTOP_ST_BOOTING,
  SHOWDUINO_ESTOP_ST_SEARCHING,
  SHOWDUINO_ESTOP_ST_NORMAL,
  SHOWDUINO_ESTOP_ST_LATCHED,
  SHOWDUINO_ESTOP_ST_NEEDS_REARM,
  SHOWDUINO_ESTOP_ST_FAULT
} ShowduinoEmergencyNodeState;

typedef enum ShowduinoEmergencyNodeCmd {
  SHOWDUINO_ESTOP_CMD_NONE = 0,
  SHOWDUINO_ESTOP_CMD_STATUS,
  SHOWDUINO_ESTOP_CMD_OWN_GRANT,
  SHOWDUINO_ESTOP_CMD_ACK_LATCHED,
  SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED,
  SHOWDUINO_ESTOP_CMD_REARM,
  SHOWDUINO_ESTOP_CMD_ID,
  SHOWDUINO_ESTOP_CMD_NAME,
  SHOWDUINO_ESTOP_CMD_REJECT_CLEAR
} ShowduinoEmergencyNodeCmd;

typedef enum ShowduinoEmergencyInput {
  SHOWDUINO_ESTOP_IN_RELEASED = 0, /* momentary button released (HIGH) */
  SHOWDUINO_ESTOP_IN_PRESSED = 1   /* momentary button pressed (LOW) */
} ShowduinoEmergencyInput;

/* Wire field input_open remains 1=active for Protocol 1.0 binary compatibility.
 * Operator surfaces must display PRESSED/RELEASED, not OPEN/CLOSED. */
#define SHOWDUINO_ESTOP_IN_CLOSED SHOWDUINO_ESTOP_IN_RELEASED
#define SHOWDUINO_ESTOP_IN_OPEN   SHOWDUINO_ESTOP_IN_PRESSED

typedef struct ShowduinoEmergencyAnnounce {
  char mac[18];
  char firmware[12];
  char state[20];
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
  uint8_t input_open; /* 1 = button pressed (legacy field name) */
  uint8_t latched;
  uint8_t acked;
} ShowduinoEmergencyAnnounce;

typedef struct ShowduinoEmergencyRoute {
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  uint32_t sequence;
  const char *command;
} ShowduinoEmergencyRoute;

typedef struct ShowduinoEmergencyMachine {
  uint8_t latched;
  uint8_t input_open;
  uint8_t acked;
  uint8_t global_clear_seen;
  uint8_t radio_up;
  uint8_t pending_assert;
  uint8_t burst_left;
  uint32_t next_tx_ms;
  uint32_t last_assert_log_ms;
  ShowduinoEmergencyNodeState state;
} ShowduinoEmergencyMachine;

typedef struct ShowduinoEmergencySourceRec {
  uint8_t used;
  uint8_t asserting;
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
} ShowduinoEmergencySourceRec;

typedef struct ShowduinoEmergencyAuthority {
  uint8_t locked;
  uint8_t hardwired;
  uint8_t wireless;
  uint8_t remote;
  uint8_t usb;
  uint8_t offline_fault;
  char primary_id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char primary_name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
  char primary_kind[12];
  ShowduinoEmergencySourceRec stations[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
} ShowduinoEmergencyAuthority;

static inline const char *showduino_emergency_state_name(ShowduinoEmergencyNodeState st) {
  switch (st) {
    case SHOWDUINO_ESTOP_ST_BOOTING: return "BOOTING";
    case SHOWDUINO_ESTOP_ST_SEARCHING: return "SEARCHING";
    case SHOWDUINO_ESTOP_ST_NORMAL: return "NORMAL";
    case SHOWDUINO_ESTOP_ST_LATCHED: return "LATCHED";
    case SHOWDUINO_ESTOP_ST_NEEDS_REARM: return "NEEDS_REARM";
    case SHOWDUINO_ESTOP_ST_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static inline int showduino_emergency_id_ok(const char *id) {
  size_t n;
  const char *p;
  if (!id) return 0;
  if (strncmp(id, "ESTOP-", 6) != 0) return 0;
  p = id + 6;
  if (*p < '0' || *p > '9') return 0;
  n = 0;
  while (*p >= '0' && *p <= '9' && n < 4) {
    p++;
    n++;
  }
  return *p == 0 && n >= 1;
}

static inline int showduino_emergency_name_ok(const char *name) {
  size_t n;
  const char *p;
  if (!name || !name[0]) return 0;
  n = strlen(name);
  if (n > SHOWDUINO_EMERGENCY_NAME_MAX) return 0;
  for (p = name; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c > 126) return 0;
    if (c == ':' || c == '"' || c == '\\') return 0;
  }
  return 1;
}

static inline int showduino_emergency_id_equal(const char *a, const char *b) {
  if (!a || !b) return 0;
  return strcmp(a, b) == 0;
}

static inline int showduino_emergency_is_clear_token(const char *s) {
  if (!s || !s[0]) return 0;
  if (strcmp(s, "EMERGENCY:CLEAR") == 0) return 1;
  if (strcmp(s, "EMERGENCY:CLEAR_CONFIRM") == 0) return 1;
  if (strcmp(s, "EMERGENCY:CLEAR_CANCEL") == 0) return 1;
  if (strcmp(s, "EMERGENCY:CLEAR_REQUEST") == 0) return 1;
  if (strcmp(s, "ESTOP:CLEAR") == 0) return 1;
  if (strcmp(s, "E-STOP:CLEAR") == 0) return 1;
  if (strncmp(s, "ESTOP:CLEAR", 11) == 0) return 1;
  if (strstr(s, "CLEAR_GLOBAL") != NULL) return 1;
  return 0;
}

static inline int showduino_emergency_is_assert(const char *s) {
  if (!s || !s[0]) return 0;
  if (!strncmp(s, SHOWDUINO_ESTOP_ASSERT_PREFIX, strlen(SHOWDUINO_ESTOP_ASSERT_PREFIX))) {
    return 1;
  }
  if (!strcmp(s, "ESTOP:ASSERT")) return 1;
  return 0;
}

static inline ShowduinoEmergencyNodeCmd showduino_emergency_parse_command(const char *raw) {
  const char *cmd = raw;
  if (!cmd || !cmd[0]) return SHOWDUINO_ESTOP_CMD_NONE;
  /* P4 fan-out CLEAR is observed only. It is never a node-originated clear. */
  if (!strcmp(cmd, "EMERGENCY:CLEAR")) return SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED;
  if (showduino_emergency_is_clear_token(cmd)) return SHOWDUINO_ESTOP_CMD_REJECT_CLEAR;
  if (!strncmp(cmd, "EMERGENCY:NODE:", 15)) cmd += 15;
  if (!strncmp(cmd, "ESTOP:NODE:", 11)) cmd += 11;
  if (!strncmp(cmd, "ESTOP:", 6)) cmd += 6;
  if (!strcmp(cmd, "STATUS")) return SHOWDUINO_ESTOP_CMD_STATUS;
  if (!strcmp(cmd, "OWN:GRANT")) return SHOWDUINO_ESTOP_CMD_OWN_GRANT;
  if (!strcmp(cmd, "ACK:LATCHED")) return SHOWDUINO_ESTOP_CMD_ACK_LATCHED;
  if (!strcmp(cmd, "GLOBAL:OBSERVED")) return SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED;
  if (!strcmp(cmd, "REARM")) return SHOWDUINO_ESTOP_CMD_REARM;
  if (!strncmp(cmd, "ID:", 3)) return SHOWDUINO_ESTOP_CMD_ID;
  if (!strncmp(cmd, "NAME:", 5)) return SHOWDUINO_ESTOP_CMD_NAME;
  if (!strcmp(raw, "EMERGENCY:CLEAR") || !strcmp(raw, "EMERGENCY:STOP")) {
    /* Fan-out STOP/CLEAR from P4 is observed globally; never a node-originated clear. */
    if (!strcmp(raw, "EMERGENCY:CLEAR")) return SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED;
    return SHOWDUINO_ESTOP_CMD_NONE;
  }
  return SHOWDUINO_ESTOP_CMD_NONE;
}

static inline int showduino_emergency_format_assert(const char *id, const char *name,
                                                    char *out, size_t n) {
  if (!out || n < 16) return 0;
  if (!showduino_emergency_id_ok(id)) id = SHOWDUINO_EMERGENCY_ID_DEFAULT;
  if (!name || !name[0] || !showduino_emergency_name_ok(name)) name = "-";
  snprintf(out, n, "ESTOP:ASSERT:%s:%s", id, name);
  return 1;
}

static inline int showduino_emergency_parse_assert(const char *line,
                                                   char *id, size_t idn,
                                                   char *name, size_t namen) {
  const char *p;
  const char *colon;
  size_t n;
  if (id && idn) id[0] = 0;
  if (name && namen) name[0] = 0;
  if (!line || strncmp(line, SHOWDUINO_ESTOP_ASSERT_PREFIX,
                       strlen(SHOWDUINO_ESTOP_ASSERT_PREFIX)) != 0) {
    return 0;
  }
  p = line + strlen(SHOWDUINO_ESTOP_ASSERT_PREFIX);
  colon = strchr(p, ':');
  n = colon ? (size_t)(colon - p) : strlen(p);
  if (id && idn) {
    if (n >= idn) n = idn - 1;
    memcpy(id, p, n);
    id[n] = 0;
  }
  if (colon && colon[1] && name && namen) {
    strncpy(name, colon + 1, namen - 1);
    name[namen - 1] = 0;
  }
  return showduino_emergency_id_ok(id);
}

static inline int showduino_emergency_button_pressed(const ShowduinoEmergencyMachine *m) {
  return m && m->input_open != 0;
}

static inline const char *showduino_emergency_button_name(int pressed) {
  return pressed ? "PRESSED" : "RELEASED";
}

static inline void showduino_emergency_machine_init(ShowduinoEmergencyMachine *m,
                                                    int button_pressed) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  m->input_open = button_pressed ? 1 : 0;
  m->state = SHOWDUINO_ESTOP_ST_BOOTING;
  if (button_pressed) {
    m->latched = 1;
    m->pending_assert = 1;
    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;
    m->state = SHOWDUINO_ESTOP_ST_LATCHED;
  } else {
    m->state = SHOWDUINO_ESTOP_ST_NORMAL;
  }
}

static inline void showduino_emergency_machine_set_radio(ShowduinoEmergencyMachine *m,
                                                         int up) {
  if (!m) return;
  if (up && !m->radio_up && m->latched) {
    m->pending_assert = 1;
    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;
    m->next_tx_ms = 0;
  }
  m->radio_up = up ? 1 : 0;
}

static inline void showduino_emergency_machine_input(ShowduinoEmergencyMachine *m,
                                                     int button_pressed) {
  if (!m) return;
  m->input_open = button_pressed ? 1 : 0;
  if (button_pressed && !m->latched) {
    m->latched = 1;
    m->acked = 0;
    m->global_clear_seen = 0;
    m->pending_assert = 1;
    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;
    m->next_tx_ms = 0;
    m->state = SHOWDUINO_ESTOP_ST_LATCHED;
  }
  /* Button release never clears the local latch or P4 emergency. */
}

static inline void showduino_emergency_machine_ack(ShowduinoEmergencyMachine *m) {
  if (!m) return;
  m->acked = 1;
  /* ACK is not a clear. */
  if (m->latched) m->state = SHOWDUINO_ESTOP_ST_LATCHED;
}

/* Observe authoritative P4 clear. Never originates clear.
 * Released button → local NORMAL automatically.
 * Still pressed → remain latched and re-assert. */
static inline void showduino_emergency_machine_global_observed(ShowduinoEmergencyMachine *m) {
  if (!m) return;
  m->acked = 0;
  if (m->input_open) {
    m->global_clear_seen = 0;
    m->latched = 1;
    m->pending_assert = 1;
    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;
    m->next_tx_ms = 0;
    m->state = SHOWDUINO_ESTOP_ST_LATCHED;
    return;
  }
  m->global_clear_seen = 1;
  m->pending_assert = 0;
  m->burst_left = 0;
  m->latched = 0;
  m->state = SHOWDUINO_ESTOP_ST_NORMAL;
  m->global_clear_seen = 0;
}

/* Maintenance-only local reset. Never clears P4. Prefer auto path above. */
static inline int showduino_emergency_machine_rearm(ShowduinoEmergencyMachine *m) {
  if (!m) return 0;
  if (m->input_open) {
    m->latched = 1;
    m->pending_assert = 1;
    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;
    m->acked = 0;
    m->global_clear_seen = 0;
    m->state = SHOWDUINO_ESTOP_ST_LATCHED;
    return 0;
  }
  if (!m->latched && m->state == SHOWDUINO_ESTOP_ST_NORMAL) return 1;
  if (!m->global_clear_seen && m->latched) return 0;
  m->latched = 0;
  m->acked = 0;
  m->pending_assert = 0;
  m->burst_left = 0;
  m->global_clear_seen = 0;
  m->state = SHOWDUINO_ESTOP_ST_NORMAL;
  return 1;
}

static inline int showduino_emergency_machine_want_tx(const ShowduinoEmergencyMachine *m,
                                                      uint32_t now_ms) {
  if (!m || !m->latched || !m->radio_up) return 0;
  if (m->global_clear_seen) return 0;
  if (m->pending_assert) return 1;
  return now_ms >= m->next_tx_ms;
}

static inline void showduino_emergency_machine_sent(ShowduinoEmergencyMachine *m,
                                                    uint32_t now_ms) {
  if (!m) return;
  m->pending_assert = 0;
  if (m->burst_left) {
    m->burst_left--;
    m->next_tx_ms = now_ms + SHOWDUINO_EMERGENCY_BURST_MS;
  } else {
    m->next_tx_ms = now_ms + SHOWDUINO_EMERGENCY_HEARTBEAT_MS;
  }
}

static inline int showduino_emergency_should_log_assert(ShowduinoEmergencyMachine *m,
                                                        uint32_t now_ms) {
  if (!m) return 0;
  if (!m->last_assert_log_ms || (now_ms - m->last_assert_log_ms) >= 2000u) {
    m->last_assert_log_ms = now_ms;
    return 1;
  }
  return 0;
}

static inline int showduino_emergency_parse_announce(const char *line,
                                                     ShowduinoEmergencyAnnounce *out) {
  const char *p;
  const char *colon;
  size_t n;
  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (strncmp(line, "ANNOUNCE:", 9) != 0) return 0;
  p = line + 9;
  if (strlen(p) < 17) return 0;
  memcpy(out->mac, p, 17);
  out->mac[17] = '\0';
  p += 17;
  if (*p != ':') return 0;
  p++;
  colon = strchr(p, ':');
  if (!colon) return 0;
  n = (size_t)(colon - p);
  if (n >= sizeof(out->firmware)) n = sizeof(out->firmware) - 1;
  memcpy(out->firmware, p, n);
  p = colon + 1;
  colon = strchr(p, ':');
  if (!colon) {
    strncpy(out->state, p, sizeof(out->state) - 1);
    return 1;
  }
  n = (size_t)(colon - p);
  if (n >= sizeof(out->state)) n = sizeof(out->state) - 1;
  memcpy(out->state, p, n);
  p = colon + 1;
  while (p && *p) {
    if (!strncmp(p, "ID=", 3)) {
      p += 3;
      colon = strchr(p, ':');
      n = colon ? (size_t)(colon - p) : strlen(p);
      if (n > SHOWDUINO_EMERGENCY_ID_MAX) n = SHOWDUINO_EMERGENCY_ID_MAX;
      memcpy(out->id, p, n);
      out->id[n] = '\0';
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "N=", 2)) {
      p += 2;
      colon = strchr(p, ':');
      n = colon ? (size_t)(colon - p) : strlen(p);
      if (n > SHOWDUINO_EMERGENCY_NAME_MAX) n = SHOWDUINO_EMERGENCY_NAME_MAX;
      memcpy(out->name, p, n);
      out->name[n] = '\0';
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "IN=", 3)) {
      /* PRESSED/OPEN = active; RELEASED/CLOSED = idle (compat). */
      out->input_open = (uint8_t)((!strncmp(p + 3, "PRESSED", 7) ||
                                   !strncmp(p + 3, "OPEN", 4)) ? 1 : 0);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "L=", 2)) {
      out->latched = (uint8_t)(p[2] == '1' ? 1 : 0);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else if (!strncmp(p, "A=", 2)) {
      out->acked = (uint8_t)(p[2] == '1' ? 1 : 0);
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    } else {
      colon = strchr(p, ':');
      p = colon ? colon + 1 : NULL;
    }
  }
  return 1;
}

static inline int showduino_emergency_format_announce(const ShowduinoEmergencyAnnounce *in,
                                                      char *out, size_t n) {
  if (!in || !out || n < 24) return 0;
  snprintf(out, n, "ANNOUNCE:%s:%s:%s:ID=%s:N=%s:IN=%s:L=%u:A=%u",
           in->mac[0] ? in->mac : "00:00:00:00:00:00",
           in->firmware[0] ? in->firmware : "0.0.0",
           in->state[0] ? in->state : "UNKNOWN",
           in->id[0] ? in->id : SHOWDUINO_EMERGENCY_ID_DEFAULT,
           in->name[0] ? in->name : "-",
           showduino_emergency_button_name(in->input_open),
           (unsigned)in->latched,
           (unsigned)in->acked);
  return 1;
}

static inline int showduino_emergency_parse_route(const char *line,
                                                  ShowduinoEmergencyRoute *out) {
  const char *p;
  const char *c1;
  const char *c2;
  size_t n;
  if (!line || !out) return 0;
  memset(out, 0, sizeof(*out));
  if (strncmp(line, SHOWDUINO_LEGACY_ROUTE_EMERGENCY,
              strlen(SHOWDUINO_LEGACY_ROUTE_EMERGENCY)) != 0) {
    return 0;
  }
  p = line + strlen(SHOWDUINO_LEGACY_ROUTE_EMERGENCY);
  c1 = strchr(p, ':');
  if (!c1 || c1 == p) return 0;
  n = (size_t)(c1 - p);
  if (n > SHOWDUINO_EMERGENCY_ID_MAX) return 0;
  memcpy(out->id, p, n);
  out->id[n] = '\0';
  if (!showduino_emergency_id_ok(out->id)) return 0;
  c2 = strchr(c1 + 1, ':');
  if (!c2) return 0;
  out->sequence = (uint32_t)strtoul(c1 + 1, NULL, 10);
  out->command = c2 + 1;
  return out->command[0] ? 1 : 0;
}

static inline void showduino_emergency_authority_init(ShowduinoEmergencyAuthority *a) {
  if (!a) return;
  memset(a, 0, sizeof(*a));
}

static inline ShowduinoEmergencySourceRec *showduino_emergency_authority_find(
    ShowduinoEmergencyAuthority *a, const char *id) {
  uint8_t i;
  if (!a || !id || !id[0]) return NULL;
  for (i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; i++) {
    if (a->stations[i].used && showduino_emergency_id_equal(a->stations[i].id, id)) {
      return &a->stations[i];
    }
  }
  return NULL;
}

static inline ShowduinoEmergencySourceRec *showduino_emergency_authority_alloc(
    ShowduinoEmergencyAuthority *a, const char *id) {
  uint8_t i;
  ShowduinoEmergencySourceRec *hit = showduino_emergency_authority_find(a, id);
  if (hit) return hit;
  for (i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; i++) {
    if (!a->stations[i].used) {
      memset(&a->stations[i], 0, sizeof(a->stations[i]));
      a->stations[i].used = 1;
      strncpy(a->stations[i].id, id, SHOWDUINO_EMERGENCY_ID_MAX);
      return &a->stations[i];
    }
  }
  return NULL;
}

/* Returns 1 if this assert should latch global emergency for the first time. */
static inline int showduino_emergency_authority_assert(ShowduinoEmergencyAuthority *a,
                                                       const char *id,
                                                       const char *name) {
  ShowduinoEmergencySourceRec *st;
  int first;
  if (!a || !showduino_emergency_id_ok(id)) return 0;
  st = showduino_emergency_authority_alloc(a, id);
  if (!st) return 0;
  if (name && showduino_emergency_name_ok(name)) {
    strncpy(st->name, name, SHOWDUINO_EMERGENCY_NAME_MAX);
  }
  st->asserting = 1;
  first = !a->locked;
  a->locked = 1;
  a->wireless = 1;
  if (first) {
    strncpy(a->primary_id, id, SHOWDUINO_EMERGENCY_ID_MAX);
    strncpy(a->primary_name, st->name, SHOWDUINO_EMERGENCY_NAME_MAX);
    strncpy(a->primary_kind, "wireless", sizeof(a->primary_kind) - 1);
  }
  return first;
}

static inline void showduino_emergency_authority_hardwired(ShowduinoEmergencyAuthority *a) {
  if (!a) return;
  a->locked = 1;
  a->hardwired = 1;
  if (!a->primary_id[0]) {
    strncpy(a->primary_kind, "hardwired", sizeof(a->primary_kind) - 1);
    a->primary_id[0] = 0;
    a->primary_name[0] = 0;
  }
}

static inline int showduino_emergency_authority_apply_node_clear(
    ShowduinoEmergencyAuthority *a, const char *raw) {
  (void)a;
  (void)raw;
  /* Wireless / node-originated clear is illegal. Never unlocks. */
  return 0;
}

static inline void showduino_emergency_authority_legitimate_clear(
    ShowduinoEmergencyAuthority *a) {
  uint8_t i;
  if (!a) return;
  a->locked = 0;
  a->hardwired = 0;
  a->wireless = 0;
  a->remote = 0;
  a->usb = 0;
  a->primary_id[0] = 0;
  a->primary_name[0] = 0;
  a->primary_kind[0] = 0;
  for (i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; i++) {
    a->stations[i].asserting = 0;
  }
}

/*
 * Sequential multi-station update / commissioning:
 *   ESTOP-01 update -> reboot -> HEALTHY + LINKED -> ESTOP-02 update -> ...
 * Never flash or reboot two Emergency Nodes at the same time.
 * HEALTHY = booted, input readable, no local fault.
 * LINKED  = Comms/P4 have a fresh announce/heartbeat for that logical ID.
 */
typedef struct ShowduinoEmergencyUpdateGate {
  char current_id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char next_id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  uint8_t current_healthy;
  uint8_t current_linked;
  uint8_t allow_next;
  char hold_reason[72];
} ShowduinoEmergencyUpdateGate;

static inline int showduino_emergency_id_number(const char *id) {
  if (!showduino_emergency_id_ok(id)) return 0;
  return atoi(id + 6);
}

static inline int showduino_emergency_format_id(int n, char *out, size_t cap) {
  if (!out || cap < 9 || n < 1 || n > 99) return 0;
  snprintf(out, cap, "ESTOP-%02d", n);
  return showduino_emergency_id_ok(out);
}

static inline int showduino_emergency_next_id(const char *current, char *out, size_t cap) {
  int n = showduino_emergency_id_number(current);
  if (n < 1) return 0;
  return showduino_emergency_format_id(n + 1, out, cap);
}

static inline int showduino_emergency_update_next_allowed(int healthy, int linked) {
  return (healthy && linked) ? 1 : 0;
}

static inline void showduino_emergency_update_gate(ShowduinoEmergencyUpdateGate *g,
                                                   const char *current_id,
                                                   int healthy,
                                                   int linked) {
  if (!g) return;
  memset(g, 0, sizeof(*g));
  if (showduino_emergency_id_ok(current_id)) {
    strncpy(g->current_id, current_id, SHOWDUINO_EMERGENCY_ID_MAX);
  } else {
    strncpy(g->current_id, SHOWDUINO_EMERGENCY_ID_DEFAULT, SHOWDUINO_EMERGENCY_ID_MAX);
  }
  g->current_healthy = healthy ? 1 : 0;
  g->current_linked = linked ? 1 : 0;
  showduino_emergency_next_id(g->current_id, g->next_id, sizeof(g->next_id));
  if (showduino_emergency_update_next_allowed(healthy, linked)) {
    g->allow_next = 1;
  } else {
    strncpy(g->hold_reason,
            "Hold next station until current is HEALTHY and LINKED",
            sizeof(g->hold_reason) - 1);
  }
}

static inline void showduino_emergency_authority_offline(ShowduinoEmergencyAuthority *a,
                                                         int any_offline) {
  if (!a) return;
  a->offline_fault = any_offline ? 1 : 0;
  /* Offline is never a global latch. */
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_EMERGENCY_NODE_H */
