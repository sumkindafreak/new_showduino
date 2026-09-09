#ifndef SHOWDUINO_LAMP_NODE_H
#define SHOWDUINO_LAMP_NODE_H

/*
 * Host-testable C3 Lamp Node command, FX registry, and state rules.
 * No Arduino, NeoPixel, OLED, or ESP-NOW side effects.
 *
 * Original carbide-lamp visual FX keep their names. Extra Showduino FX are
 * marked separately and must not replace the carbide set.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "showduino_node_ownership.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_LAMP_NODE_TYPE        "LAMP"
#define SHOWDUINO_LAMP_NODE_NAME        "C3 Lamp Node"
#define SHOWDUINO_LAMP_PROTOCOL         "1.0"
#define SHOWDUINO_LAMP_TOKEN_MAX        24
#define SHOWDUINO_LAMP_DISPLAY_MAX      24
#define SHOWDUINO_LAMP_BRI_MAX          100
#define SHOWDUINO_LAMP_COMMS_TIMEOUT_MS 5000u
#define SHOWDUINO_LAMP_LIST_PER_PAGE    5
#define SHOWDUINO_LAMP_CAPS \
  "CARBIDE_FX,SHOWDUINO_FX,BRI,OLED,ESPNOW,EMERGENCY,STANDALONE,OWN"

typedef enum ShowduinoLampOrigin {
  SHOWDUINO_LAMP_FX_CARBIDE = 0,
  SHOWDUINO_LAMP_FX_SHOWDUINO = 1
} ShowduinoLampOrigin;

typedef enum ShowduinoLampFx {
  SHOWDUINO_LAMP_FX_CARBIDE_FLAME = 0,
  SHOWDUINO_LAMP_FX_CARBIDE_FLUTTER,
  SHOWDUINO_LAMP_FX_MINER_FLAME,
  SHOWDUINO_LAMP_FX_GAS_LEAK,
  SHOWDUINO_LAMP_FX_HEADLAMP_BEAM,
  SHOWDUINO_LAMP_FX_OLD_FLAME,
  SHOWDUINO_LAMP_FX_CAVE_EXPLORER,
  SHOWDUINO_LAMP_FX_LOW_FUEL,
  SHOWDUINO_LAMP_FX_CANARY_WARNING,
  SHOWDUINO_LAMP_FX_FLICKER,
  SHOWDUINO_LAMP_FX_GLOW,
  SHOWDUINO_LAMP_FX_COLOR_SHIFT,
  SHOWDUINO_LAMP_FX_STROBE,
  SHOWDUINO_LAMP_FX_POLICE_STROBE,
  SHOWDUINO_LAMP_FX_CARBIDE_COUNT,
  SHOWDUINO_LAMP_FX_SOLID = SHOWDUINO_LAMP_FX_CARBIDE_COUNT,
  SHOWDUINO_LAMP_FX_FADE_IN,
  SHOWDUINO_LAMP_FX_FADE_OUT,
  SHOWDUINO_LAMP_FX_PULSE,
  SHOWDUINO_LAMP_FX_BREATHE,
  SHOWDUINO_LAMP_FX_CANDLE,
  SHOWDUINO_LAMP_FX_FIRE,
  SHOWDUINO_LAMP_FX_RANDOM_FLICKER,
  SHOWDUINO_LAMP_FX_FAULT_FLICKER,
  SHOWDUINO_LAMP_FX_COUNT
} ShowduinoLampFx;

typedef enum ShowduinoLampNodeState {
  SHOWDUINO_LAMP_ST_UNKNOWN = 0,
  SHOWDUINO_LAMP_ST_BOOTING,
  SHOWDUINO_LAMP_ST_SEARCHING,
  SHOWDUINO_LAMP_ST_STANDALONE,
  SHOWDUINO_LAMP_ST_SHOW_CONTROLLED,
  SHOWDUINO_LAMP_ST_EMERGENCY,
  SHOWDUINO_LAMP_ST_FAULT
} ShowduinoLampNodeState;

typedef enum ShowduinoLampCmd {
  SHOWDUINO_LAMP_CMD_NONE = 0,
  SHOWDUINO_LAMP_CMD_STATUS,
  SHOWDUINO_LAMP_CMD_OFF,
  SHOWDUINO_LAMP_CMD_STOP,
  SHOWDUINO_LAMP_CMD_SOLID,
  SHOWDUINO_LAMP_CMD_FX,
  SHOWDUINO_LAMP_CMD_BRIGHTNESS,
  SHOWDUINO_LAMP_CMD_TEST,
  SHOWDUINO_LAMP_CMD_LIST,
  SHOWDUINO_LAMP_CMD_OWN_GRANT,
  SHOWDUINO_LAMP_CMD_EMERGENCY_STOP,
  SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR,
  SHOWDUINO_LAMP_CMD_LOCAL_REJECT
} ShowduinoLampCmd;

typedef enum ShowduinoLampFail {
  SHOWDUINO_LAMP_FAIL_NONE = 0,
  SHOWDUINO_LAMP_FAIL_EMERGENCY,
  SHOWDUINO_LAMP_FAIL_BAD_COMMAND,
  SHOWDUINO_LAMP_FAIL_UNKNOWN_FX,
  SHOWDUINO_LAMP_FAIL_COMMS_TIMEOUT,
  SHOWDUINO_LAMP_FAIL_SHOW_CONTROLLED,
  SHOWDUINO_LAMP_FAIL_NOT_OWNER
} ShowduinoLampFail;

typedef struct ShowduinoLampFxInfo {
  ShowduinoLampFx id;
  const char *token;
  const char *display;
  ShowduinoLampOrigin origin;
  uint16_t intervalMs;
} ShowduinoLampFxInfo;

typedef struct ShowduinoLampCommand {
  ShowduinoLampCmd cmd;
  ShowduinoLampFx fx;
  uint8_t brightness; /* 0..100, 255 = unchanged */
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t speed;       /* 1..100, 0 = default */
  uint8_t intensity;   /* 0..100, 255 = unchanged */
  uint8_t randomness;  /* 0..100, 255 = unchanged; carbide FX ignore this */
} ShowduinoLampCommand;

static const ShowduinoLampFxInfo SHOWDUINO_LAMP_FX_TABLE[] = {
  { SHOWDUINO_LAMP_FX_CARBIDE_FLAME,  "CARBIDE_FLAME",  "Carbide Flame",  SHOWDUINO_LAMP_FX_CARBIDE, 80 },
  { SHOWDUINO_LAMP_FX_CARBIDE_FLUTTER,"CARBIDE_FLUTTER","Carbide Flutter",SHOWDUINO_LAMP_FX_CARBIDE, 50 },
  { SHOWDUINO_LAMP_FX_MINER_FLAME,    "MINER_FLAME",    "Miner Flame",    SHOWDUINO_LAMP_FX_CARBIDE, 40 },
  { SHOWDUINO_LAMP_FX_GAS_LEAK,       "GAS_LEAK",       "Gas Leak",       SHOWDUINO_LAMP_FX_CARBIDE, 150 },
  { SHOWDUINO_LAMP_FX_HEADLAMP_BEAM,  "HEADLAMP_BEAM",  "Headlamp Beam",  SHOWDUINO_LAMP_FX_CARBIDE, 100 },
  { SHOWDUINO_LAMP_FX_OLD_FLAME,      "OLD_FLAME",      "Old Flame",      SHOWDUINO_LAMP_FX_CARBIDE, 90 },
  { SHOWDUINO_LAMP_FX_CAVE_EXPLORER,  "CAVE_EXPLORER",  "Cave Explorer",  SHOWDUINO_LAMP_FX_CARBIDE, 30 },
  { SHOWDUINO_LAMP_FX_LOW_FUEL,       "LOW_FUEL",       "Low Fuel",       SHOWDUINO_LAMP_FX_CARBIDE, 100 },
  { SHOWDUINO_LAMP_FX_CANARY_WARNING, "CANARY_WARNING", "Canary Warning", SHOWDUINO_LAMP_FX_CARBIDE, 50 },
  { SHOWDUINO_LAMP_FX_FLICKER,        "FLICKER",        "Flicker",        SHOWDUINO_LAMP_FX_CARBIDE, 100 },
  { SHOWDUINO_LAMP_FX_GLOW,           "GLOW",           "Glow",           SHOWDUINO_LAMP_FX_CARBIDE, 20 },
  { SHOWDUINO_LAMP_FX_COLOR_SHIFT,    "COLOR_SHIFT",    "Color Shift",    SHOWDUINO_LAMP_FX_CARBIDE, 30 },
  { SHOWDUINO_LAMP_FX_STROBE,         "STROBE",         "Strobe",         SHOWDUINO_LAMP_FX_CARBIDE, 50 },
  { SHOWDUINO_LAMP_FX_POLICE_STROBE,  "POLICE_STROBE",  "Police Strobe",  SHOWDUINO_LAMP_FX_CARBIDE, 50 },
  { SHOWDUINO_LAMP_FX_SOLID,          "SOLID",          "Solid",          SHOWDUINO_LAMP_FX_SHOWDUINO, 40 },
  { SHOWDUINO_LAMP_FX_FADE_IN,        "FADE_IN",        "Fade In",        SHOWDUINO_LAMP_FX_SHOWDUINO, 20 },
  { SHOWDUINO_LAMP_FX_FADE_OUT,       "FADE_OUT",       "Fade Out",       SHOWDUINO_LAMP_FX_SHOWDUINO, 20 },
  { SHOWDUINO_LAMP_FX_PULSE,          "PULSE",          "Pulse",          SHOWDUINO_LAMP_FX_SHOWDUINO, 20 },
  { SHOWDUINO_LAMP_FX_BREATHE,        "BREATHE",        "Breathe",        SHOWDUINO_LAMP_FX_SHOWDUINO, 20 },
  { SHOWDUINO_LAMP_FX_CANDLE,         "CANDLE",         "Candle",         SHOWDUINO_LAMP_FX_SHOWDUINO, 40 },
  { SHOWDUINO_LAMP_FX_FIRE,           "FIRE",           "Fire",           SHOWDUINO_LAMP_FX_SHOWDUINO, 40 },
  { SHOWDUINO_LAMP_FX_RANDOM_FLICKER, "RANDOM_FLICKER", "Random Flicker", SHOWDUINO_LAMP_FX_SHOWDUINO, 50 },
  { SHOWDUINO_LAMP_FX_FAULT_FLICKER,  "FAULT_FLICKER",  "Fault Flicker",  SHOWDUINO_LAMP_FX_SHOWDUINO, 80 }
};

#define SHOWDUINO_LAMP_FX_TABLE_LEN \
  (sizeof(SHOWDUINO_LAMP_FX_TABLE) / sizeof(SHOWDUINO_LAMP_FX_TABLE[0]))

static inline const char *showduino_lamp_state_name(ShowduinoLampNodeState st) {
  switch (st) {
    case SHOWDUINO_LAMP_ST_BOOTING: return "BOOTING";
    case SHOWDUINO_LAMP_ST_SEARCHING: return "SEARCHING";
    case SHOWDUINO_LAMP_ST_STANDALONE: return "STANDALONE";
    case SHOWDUINO_LAMP_ST_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_LAMP_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_LAMP_ST_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

static inline ShowduinoLampNodeState showduino_lamp_state_from_owner(
    ShowduinoNodeOwnerMode m) {
  switch (m) {
    case SHOWDUINO_OWNER_BOOTING: return SHOWDUINO_LAMP_ST_BOOTING;
    case SHOWDUINO_OWNER_SEARCHING: return SHOWDUINO_LAMP_ST_SEARCHING;
    case SHOWDUINO_OWNER_STANDALONE: return SHOWDUINO_LAMP_ST_STANDALONE;
    case SHOWDUINO_OWNER_SHOW_CONTROLLED: return SHOWDUINO_LAMP_ST_SHOW_CONTROLLED;
    case SHOWDUINO_OWNER_EMERGENCY: return SHOWDUINO_LAMP_ST_EMERGENCY;
    case SHOWDUINO_OWNER_FAULT: return SHOWDUINO_LAMP_ST_FAULT;
    default: return SHOWDUINO_LAMP_ST_UNKNOWN;
  }
}

static inline const ShowduinoLampFxInfo *showduino_lamp_fx_info(ShowduinoLampFx id) {
  size_t i;
  for (i = 0; i < SHOWDUINO_LAMP_FX_TABLE_LEN; ++i) {
    if (SHOWDUINO_LAMP_FX_TABLE[i].id == id) return &SHOWDUINO_LAMP_FX_TABLE[i];
  }
  return NULL;
}

static inline int showduino_lamp_fx_from_token(const char *token, ShowduinoLampFx *out) {
  size_t i;
  if (!token || !token[0] || !out) return -1;
  for (i = 0; i < SHOWDUINO_LAMP_FX_TABLE_LEN; ++i) {
    if (strcmp(SHOWDUINO_LAMP_FX_TABLE[i].token, token) == 0) {
      *out = SHOWDUINO_LAMP_FX_TABLE[i].id;
      return 0;
    }
  }
  return -1;
}

static inline uint8_t showduino_lamp_clamp_bri(int v) {
  if (v < 0) return 0;
  if (v > SHOWDUINO_LAMP_BRI_MAX) return SHOWDUINO_LAMP_BRI_MAX;
  return (uint8_t)v;
}

static inline int showduino_lamp_eqi(const char *a, const char *b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    char ca = (char)toupper((unsigned char)*a++);
    char cb = (char)toupper((unsigned char)*b++);
    if (ca != cb) return 0;
  }
  return *a == 0 && *b == 0;
}

static inline const char *showduino_lamp_strip_prefix(const char *cmd) {
  if (!cmd) return cmd;
  if (strncmp(cmd, "LAMP:NODE:", 10) == 0) return cmd + 10;
  if (strncmp(cmd, "LAMP:", 5) == 0) return cmd;
  return cmd;
}

static inline int showduino_lamp_parse_u8(const char *s, uint8_t *out, uint8_t maxv) {
  unsigned v = 0;
  if (!s || !out || !s[0]) return -1;
  while (*s >= '0' && *s <= '9') {
    v = v * 10u + (unsigned)(*s - '0');
    if (v > 255u) return -1;
    s++;
  }
  if (*s != 0 && *s != ',' && *s != ':' && *s != '=') return -1;
  if (v > maxv) return -1;
  *out = (uint8_t)v;
  return 0;
}

static inline ShowduinoLampCmd showduino_lamp_parse_command(
    const char *raw,
    ShowduinoLampCommand *out) {
  const char *cmd;
  ShowduinoLampCommand tmp;
  memset(&tmp, 0, sizeof(tmp));
  tmp.brightness = 255;
  tmp.intensity = 255;
  tmp.randomness = 255;
  tmp.r = 255;
  tmp.g = 180;
  tmp.b = 40;
  if (out) *out = tmp;
  if (!raw || !raw[0]) return SHOWDUINO_LAMP_CMD_NONE;

  if (strcmp(raw, "EMERGENCY:STOP") == 0) {
    tmp.cmd = SHOWDUINO_LAMP_CMD_EMERGENCY_STOP;
    if (out) *out = tmp;
    return tmp.cmd;
  }
  if (strcmp(raw, "EMERGENCY:CLEAR") == 0) {
    tmp.cmd = SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR;
    if (out) *out = tmp;
    return tmp.cmd;
  }

  cmd = showduino_lamp_strip_prefix(raw);
  if (strncmp(cmd, "LAMP:", 5) == 0) {
    cmd += 5;
  } else if (strncmp(raw, "LAMP:NODE:", 10) != 0) {
    return SHOWDUINO_LAMP_CMD_NONE;
  }

  if (strcmp(cmd, "STATUS") == 0) tmp.cmd = SHOWDUINO_LAMP_CMD_STATUS;
  else if (strcmp(cmd, "OFF") == 0 || strcmp(cmd, "STOP") == 0) tmp.cmd = SHOWDUINO_LAMP_CMD_OFF;
  else if (strcmp(cmd, "TEST") == 0) tmp.cmd = SHOWDUINO_LAMP_CMD_TEST;
  else if (strcmp(cmd, "LIST") == 0) tmp.cmd = SHOWDUINO_LAMP_CMD_LIST;
  else if (strcmp(cmd, "OWN:GRANT") == 0) tmp.cmd = SHOWDUINO_LAMP_CMD_OWN_GRANT;
  else if (strncmp(cmd, "BRIGHTNESS:", 11) == 0) {
    uint8_t v = 0;
    if (showduino_lamp_parse_u8(cmd + 11, &v, SHOWDUINO_LAMP_BRI_MAX) != 0) {
      return SHOWDUINO_LAMP_CMD_NONE;
    }
    tmp.cmd = SHOWDUINO_LAMP_CMD_BRIGHTNESS;
    tmp.brightness = v;
  } else if (strncmp(cmd, "SOLID:", 6) == 0) {
    const char *p = cmd + 6;
    uint8_t r = 0, g = 0, b = 0;
    if (showduino_lamp_parse_u8(p, &r, 255) != 0) return SHOWDUINO_LAMP_CMD_NONE;
    p = strchr(p, ',');
    if (!p) return SHOWDUINO_LAMP_CMD_NONE;
    if (showduino_lamp_parse_u8(p + 1, &g, 255) != 0) return SHOWDUINO_LAMP_CMD_NONE;
    p = strchr(p + 1, ',');
    if (!p) return SHOWDUINO_LAMP_CMD_NONE;
    if (showduino_lamp_parse_u8(p + 1, &b, 255) != 0) return SHOWDUINO_LAMP_CMD_NONE;
    tmp.cmd = SHOWDUINO_LAMP_CMD_SOLID;
    tmp.fx = SHOWDUINO_LAMP_FX_SOLID;
    tmp.r = r;
    tmp.g = g;
    tmp.b = b;
  } else if (strcmp(cmd, "SOLID") == 0) {
    tmp.cmd = SHOWDUINO_LAMP_CMD_SOLID;
    tmp.fx = SHOWDUINO_LAMP_FX_SOLID;
  } else if (strncmp(cmd, "FX:", 3) == 0) {
    char token[SHOWDUINO_LAMP_TOKEN_MAX];
    size_t n = 0;
    const char *p = cmd + 3;
    ShowduinoLampFx fx;
    while (*p && *p != ':' && n + 1 < sizeof(token)) {
      token[n++] = (char)toupper((unsigned char)*p++);
    }
    token[n] = 0;
    if (showduino_lamp_fx_from_token(token, &fx) != 0) return SHOWDUINO_LAMP_CMD_NONE;
    tmp.cmd = SHOWDUINO_LAMP_CMD_FX;
    tmp.fx = fx;
    while (*p == ':') {
      p++;
      if (strncmp(p, "BRI=", 4) == 0) {
        uint8_t v = 0;
        if (showduino_lamp_parse_u8(p + 4, &v, SHOWDUINO_LAMP_BRI_MAX) != 0) {
          return SHOWDUINO_LAMP_CMD_NONE;
        }
        tmp.brightness = v;
        p = strchr(p, ':');
        if (!p) break;
      } else if (strncmp(p, "SPD=", 4) == 0) {
        uint8_t v = 0;
        if (showduino_lamp_parse_u8(p + 4, &v, 100) != 0 || v < 1) {
          return SHOWDUINO_LAMP_CMD_NONE;
        }
        tmp.speed = v;
        p = strchr(p, ':');
        if (!p) break;
      } else if (strncmp(p, "INT=", 4) == 0) {
        uint8_t v = 0;
        if (showduino_lamp_parse_u8(p + 4, &v, 100) != 0) {
          return SHOWDUINO_LAMP_CMD_NONE;
        }
        tmp.intensity = v;
        p = strchr(p, ':');
        if (!p) break;
      } else if (strncmp(p, "RAND=", 5) == 0) {
        uint8_t v = 0;
        if (showduino_lamp_parse_u8(p + 5, &v, 100) != 0) {
          return SHOWDUINO_LAMP_CMD_NONE;
        }
        tmp.randomness = v;
        p = strchr(p, ':');
        if (!p) break;
      } else {
        return SHOWDUINO_LAMP_CMD_NONE;
      }
    }
  } else {
    return SHOWDUINO_LAMP_CMD_NONE;
  }

  if (out) *out = tmp;
  return tmp.cmd;
}

static inline const char *showduino_lamp_fail_name(ShowduinoLampFail f) {
  switch (f) {
    case SHOWDUINO_LAMP_FAIL_NONE: return "NONE";
    case SHOWDUINO_LAMP_FAIL_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_LAMP_FAIL_BAD_COMMAND: return "BAD_COMMAND";
    case SHOWDUINO_LAMP_FAIL_UNKNOWN_FX: return "UNKNOWN_FX";
    case SHOWDUINO_LAMP_FAIL_COMMS_TIMEOUT: return "COMMS_TIMEOUT";
    case SHOWDUINO_LAMP_FAIL_SHOW_CONTROLLED: return "SHOW_CONTROLLED";
    case SHOWDUINO_LAMP_FAIL_NOT_OWNER: return "NOT_OWNER";
    default: return "FAULT";
  }
}

static inline int showduino_lamp_cmd_theatrical(ShowduinoLampCmd cmd) {
  return cmd == SHOWDUINO_LAMP_CMD_OFF || cmd == SHOWDUINO_LAMP_CMD_STOP ||
         cmd == SHOWDUINO_LAMP_CMD_SOLID || cmd == SHOWDUINO_LAMP_CMD_FX ||
         cmd == SHOWDUINO_LAMP_CMD_BRIGHTNESS || cmd == SHOWDUINO_LAMP_CMD_TEST;
}

static inline ShowduinoLampFail showduino_lamp_can_accept_ex(
    ShowduinoLampNodeState st, ShowduinoLampCmd cmd, ShowduinoCmdOrigin origin) {
  if (cmd == SHOWDUINO_LAMP_CMD_NONE || cmd == SHOWDUINO_LAMP_CMD_LOCAL_REJECT) {
    return SHOWDUINO_LAMP_FAIL_BAD_COMMAND;
  }
  if (cmd == SHOWDUINO_LAMP_CMD_EMERGENCY_STOP) return SHOWDUINO_LAMP_FAIL_NONE;
  if (cmd == SHOWDUINO_LAMP_CMD_STATUS || cmd == SHOWDUINO_LAMP_CMD_LIST) {
    return SHOWDUINO_LAMP_FAIL_NONE;
  }
  if (cmd == SHOWDUINO_LAMP_CMD_OWN_GRANT) {
    return origin == SHOWDUINO_CMD_ORIGIN_SHOW ? SHOWDUINO_LAMP_FAIL_NONE
                                               : SHOWDUINO_LAMP_FAIL_NOT_OWNER;
  }
  if (st == SHOWDUINO_LAMP_ST_EMERGENCY) {
    if (cmd == SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR) {
      if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) return SHOWDUINO_LAMP_FAIL_NONE;
      return SHOWDUINO_LAMP_FAIL_EMERGENCY;
    }
    return SHOWDUINO_LAMP_FAIL_EMERGENCY;
  }
  if (cmd == SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) return SHOWDUINO_LAMP_FAIL_NONE;
    if (st == SHOWDUINO_LAMP_ST_STANDALONE) return SHOWDUINO_LAMP_FAIL_NONE;
    return SHOWDUINO_LAMP_FAIL_SHOW_CONTROLLED;
  }
  if (showduino_lamp_cmd_theatrical(cmd)) {
    if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) {
      return st == SHOWDUINO_LAMP_ST_SHOW_CONTROLLED
                 ? SHOWDUINO_LAMP_FAIL_NONE
                 : SHOWDUINO_LAMP_FAIL_NOT_OWNER;
    }
    if (st == SHOWDUINO_LAMP_ST_SHOW_CONTROLLED) {
      return SHOWDUINO_LAMP_FAIL_SHOW_CONTROLLED;
    }
    if (st == SHOWDUINO_LAMP_ST_STANDALONE) return SHOWDUINO_LAMP_FAIL_NONE;
    if (st == SHOWDUINO_LAMP_ST_SEARCHING &&
        origin == SHOWDUINO_CMD_ORIGIN_LOCAL) {
      return SHOWDUINO_LAMP_FAIL_NONE;
    }
    return SHOWDUINO_LAMP_FAIL_NOT_OWNER;
  }
  return SHOWDUINO_LAMP_FAIL_NONE;
}

static inline ShowduinoLampFail showduino_lamp_can_accept(
    ShowduinoLampNodeState st, ShowduinoLampCmd cmd) {
  return showduino_lamp_can_accept_ex(st, cmd, SHOWDUINO_CMD_ORIGIN_SHOW);
}

static inline void showduino_lamp_list_slice(uint16_t page, uint16_t *start, uint16_t *count) {
  uint16_t s;
  if (!start || !count) return;
  s = (uint16_t)(page * (uint16_t)SHOWDUINO_LAMP_LIST_PER_PAGE);
  if (s >= (uint16_t)SHOWDUINO_LAMP_FX_COUNT) {
    *start = (uint16_t)SHOWDUINO_LAMP_FX_COUNT;
    *count = 0;
    return;
  }
  *start = s;
  *count = (uint16_t)((uint16_t)SHOWDUINO_LAMP_FX_COUNT - s);
  if (*count > (uint16_t)SHOWDUINO_LAMP_LIST_PER_PAGE) {
    *count = (uint16_t)SHOWDUINO_LAMP_LIST_PER_PAGE;
  }
}

static inline const char *showduino_lamp_wire_token(ShowduinoLampNodeState st, int online) {
  if (!online) return "OFFLINE";
  switch (st) {
    case SHOWDUINO_LAMP_ST_SHOW_CONTROLLED: return "SHOW";
    case SHOWDUINO_LAMP_ST_STANDALONE: return "SOLO";
    case SHOWDUINO_LAMP_ST_EMERGENCY: return "EMERGENCY";
    case SHOWDUINO_LAMP_ST_FAULT: return "FAULT";
    case SHOWDUINO_LAMP_ST_SEARCHING:
    case SHOWDUINO_LAMP_ST_BOOTING: return "SEARCH";
    default: return "IDLE";
  }
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_LAMP_NODE_H */
