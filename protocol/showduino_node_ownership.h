#ifndef SHOWDUINO_NODE_OWNERSHIP_H
#define SHOWDUINO_NODE_OWNERSHIP_H

/*
 * Host-testable specialist-node ownership / mode rules.
 * No Wi-Fi, ESP-NOW, WebServer, or GPIO side effects.
 *
 * Valid Showduino authority is a P4 ownership GRANT forwarded by the
 * Communications S3. Random ESP-NOW presence is not ownership.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_OWNER_DISCOVER_MS   8000u
#define SHOWDUINO_OWNER_KEEPALIVE_MS  8000u
#define SHOWDUINO_OWNER_SSID_MAX      24
#define SHOWDUINO_OWNER_GRANT_CMD     "OWN:GRANT"

typedef enum ShowduinoNodeOwnerMode {
  SHOWDUINO_OWNER_BOOTING = 0,
  SHOWDUINO_OWNER_SEARCHING,
  SHOWDUINO_OWNER_STANDALONE,
  SHOWDUINO_OWNER_SHOW_CONTROLLED,
  SHOWDUINO_OWNER_EMERGENCY,
  SHOWDUINO_OWNER_FAULT
} ShowduinoNodeOwnerMode;

typedef enum ShowduinoCmdOrigin {
  SHOWDUINO_CMD_ORIGIN_SHOW = 0,
  SHOWDUINO_CMD_ORIGIN_WEB,
  SHOWDUINO_CMD_ORIGIN_LOCAL
} ShowduinoCmdOrigin;

typedef enum ShowduinoOwnerEvent {
  SHOWDUINO_OWNER_EV_TICK = 0,
  SHOWDUINO_OWNER_EV_GRANT,
  SHOWDUINO_OWNER_EV_KEEP,
  SHOWDUINO_OWNER_EV_EMERGENCY_STOP,
  SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR,
  SHOWDUINO_OWNER_EV_FAULT,
  SHOWDUINO_OWNER_EV_FAULT_CLEAR
} ShowduinoOwnerEvent;

/*
 * Node-type fail-safe after P4 authority is lost.
 * MOSFET is the strictest: all outputs off, never restore.
 */
typedef enum ShowduinoNodeFailSafe {
  SHOWDUINO_FAILSAFE_LAMP_OFF = 0,
  SHOWDUINO_FAILSAFE_AUDIO_STOP,
  SHOWDUINO_FAILSAFE_PIXEL_BLACKOUT,
  SHOWDUINO_FAILSAFE_MOSFET_ALL_OFF
} ShowduinoNodeFailSafe;

typedef struct ShowduinoOwnerMachine {
  ShowduinoNodeOwnerMode mode;
  uint32_t bootMs;
  uint32_t lastGrantMs;
  uint8_t granted;
  uint8_t lostAuthority;
  uint8_t enteredStandalone;
  uint8_t enteredShow;
} ShowduinoOwnerMachine;

static inline const char *showduino_owner_mode_name(ShowduinoNodeOwnerMode m) {
  switch (m) {
    case SHOWDUINO_OWNER_BOOTING: return "BOOTING";
    case SHOWDUINO_OWNER_SEARCHING: return "SEARCHING";
    case SHOWDUINO_OWNER_STANDALONE: return "STANDALONE";
    case SHOWDUINO_OWNER_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_OWNER_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_OWNER_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static inline void showduino_owner_ssid(const char *typeToken, const uint8_t mac[6],
                                        char *out, size_t n) {
  unsigned suffix = 0;
  if (!out || n < 8) return;
  if (mac) suffix = ((unsigned)mac[4] << 8) | (unsigned)mac[5];
  snprintf(out, n, "Showduino-%s-%04X",
           (typeToken && typeToken[0]) ? typeToken : "Node", suffix);
}

static inline int showduino_owner_is_grant_cmd(const char *cmd) {
  if (!cmd) return 0;
  if (strcmp(cmd, "OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "LAMP:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "LAMP:NODE:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "AUDIO:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "AUDIO:NODE:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "PIXEL:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "PIXEL:NODE:OWN:GRANT") == 0) return 1;
  if (strcmp(cmd, "MOSFET:OWN:GRANT") == 0) return 1;
  return 0;
}

static inline void showduino_owner_begin(ShowduinoOwnerMachine *m, uint32_t nowMs) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  m->mode = SHOWDUINO_OWNER_SEARCHING;
  m->bootMs = nowMs;
}

static inline void showduino_owner_apply(ShowduinoOwnerMachine *m,
                                         ShowduinoOwnerEvent ev,
                                         uint32_t nowMs,
                                         uint32_t discoverMs,
                                         uint32_t keepMs) {
  ShowduinoNodeOwnerMode next;
  if (!m) return;
  m->enteredStandalone = 0;
  m->enteredShow = 0;
  m->lostAuthority = 0;
  next = m->mode;

  if (ev == SHOWDUINO_OWNER_EV_FAULT) {
    m->mode = SHOWDUINO_OWNER_FAULT;
    return;
  }
  if (ev == SHOWDUINO_OWNER_EV_EMERGENCY_STOP) {
    m->mode = SHOWDUINO_OWNER_EMERGENCY;
    return;
  }

  if (m->mode == SHOWDUINO_OWNER_FAULT) {
    if (ev == SHOWDUINO_OWNER_EV_FAULT_CLEAR) {
      next = m->granted ? SHOWDUINO_OWNER_SHOW_CONTROLLED : SHOWDUINO_OWNER_SEARCHING;
    } else {
      return;
    }
  } else if (m->mode == SHOWDUINO_OWNER_EMERGENCY) {
    if (ev == SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR) {
      next = m->granted ? SHOWDUINO_OWNER_SHOW_CONTROLLED : SHOWDUINO_OWNER_STANDALONE;
    } else if (ev == SHOWDUINO_OWNER_EV_GRANT) {
      m->granted = 1;
      m->lastGrantMs = nowMs;
      return;
    } else {
      return;
    }
  } else if (ev == SHOWDUINO_OWNER_EV_GRANT) {
    m->granted = 1;
    m->lastGrantMs = nowMs;
    next = SHOWDUINO_OWNER_SHOW_CONTROLLED;
  } else if (ev == SHOWDUINO_OWNER_EV_KEEP) {
    if (m->granted) m->lastGrantMs = nowMs;
  } else if (ev == SHOWDUINO_OWNER_EV_TICK) {
    if (m->mode == SHOWDUINO_OWNER_SEARCHING &&
        (nowMs - m->bootMs) >= discoverMs && !m->granted) {
      next = SHOWDUINO_OWNER_STANDALONE;
    }
    if (m->mode == SHOWDUINO_OWNER_SHOW_CONTROLLED && m->granted &&
        (nowMs - m->lastGrantMs) >= keepMs) {
      m->granted = 0;
      m->lostAuthority = 1;
      next = SHOWDUINO_OWNER_STANDALONE;
    }
  }

  if (next != m->mode) {
    if (next == SHOWDUINO_OWNER_STANDALONE) m->enteredStandalone = 1;
    if (next == SHOWDUINO_OWNER_SHOW_CONTROLLED) m->enteredShow = 1;
    m->mode = next;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_NODE_OWNERSHIP_H */
