#include "MosfetNodeLink.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

extern bool emergencyLocked;
bool sendToDirector(const String &message);
void stageCommsSendLine(const char *line);

static MosfetNodeStatus sNodes[SHOWDUINO_MOSFET_NODE_MAX_NODES];
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";

static void routeTo(const char *id, uint32_t seq, const char *cmd) {
  char line[200];
  snprintf(line, sizeof(line), "ROUTE:MOSFET:%s:%lu:%s",
           id && id[0] ? id : "MOSFET-01",
           (unsigned long)seq,
           cmd ? cmd : "");
  stageCommsSendLine(line);
}

static MosfetNodeStatus *findById(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && showduino_mosfet_id_equal(sNodes[i].id, id)) return &sNodes[i];
  }
  return nullptr;
}

static MosfetNodeStatus *findByMac(const char *mac) {
  if (!mac || strlen(mac) < 17) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && !strcasecmp(sNodes[i].mac, mac)) return &sNodes[i];
  }
  return nullptr;
}

static MosfetNodeStatus *allocSlot(const char *id, const char *mac) {
  MosfetNodeStatus *hit = findById(id);
  if (!hit) hit = findByMac(mac);
  if (hit) return hit;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used) {
      sNodes[i] = MosfetNodeStatus();
      sNodes[i].used = true;
      return &sNodes[i];
    }
  }
  return nullptr;
}

static const char *wireToken() {
  uint8_t online = 0, seen = 0, fault = 0, em = 0;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) online++;
    if (!strcmp(sNodes[i].state, "FAULT")) fault++;
    if (sNodes[i].emergency || !strcmp(sNodes[i].state, "EMERGENCY")) em++;
  }
  if (!seen || !online) return "OFFLINE";
  if (em) return "EMERGENCY";
  if (fault) return "FAULT";
  return "ONLINE";
}

static void publishState(bool force = false) {
  char line[48];
  const char *tok = wireToken();
  if (force || strcmp(sLastWire, tok) != 0) {
    snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_MOSFET_PREFIX, tok);
    if (sendToDirector(String(line))) {
      strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
      sLastWire[sizeof(sLastWire) - 1] = '\0';
    }
  }
  const char *firstId = "-";
  const char *firstSt = "OFFLINE";
  const MosfetNodeStatus *firstNode = nullptr;
  uint8_t online = 0, seen = 0;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) {
      if (!online) {
        firstNode = &sNodes[i];
        firstId = sNodes[i].id[0] ? sNodes[i].id : "-";
        firstSt = sNodes[i].state[0] ? sNodes[i].state : "ONLINE";
      }
      online++;
    }
  }
  char detail[96];
  snprintf(detail, sizeof(detail), "%s%u:%u:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_MOSFET_DETAIL_PREFIX,
           (unsigned)online, (unsigned)seen, firstId, firstSt);
  if (firstNode && firstNode->haveLevels) {
    const size_t used = strlen(detail);
    snprintf(detail + used, sizeof(detail) - used, ":O=%u,%u,%u,%u",
             firstNode->levels[0], firstNode->levels[1],
             firstNode->levels[2], firstNode->levels[3]);
  }
  if (force || strcmp(sLastDetail, detail) != 0) {
    if (sendToDirector(String(detail))) {
      strncpy(sLastDetail, detail, sizeof(sLastDetail) - 1);
    }
  }
  sPublishMs = millis();
}

static void zeroCachedLevels(MosfetNodeStatus *st) {
  if (!st) return;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_OUT_COUNT; ++i) st->levels[i] = 0;
}

static bool parseStatusLine(const char *p, MosfetNodeStatus *st) {
  /* MOSFET:STATUS:<id>:OWN=<0|1>:EM=<0|1>:O=<a>,<b>,<c>,<d> */
  if (!p || !st || strncmp(p, "MOSFET:STATUS:", 14) != 0) return false;
  const char *rest = p + 14;
  char id[SHOWDUINO_MOSFET_ID_MAX + 1] = "";
  const char *colon = strchr(rest, ':');
  if (!colon) return false;
  size_t n = (size_t)(colon - rest);
  if (n > SHOWDUINO_MOSFET_ID_MAX) return false;
  memcpy(id, rest, n);
  id[n] = '\0';
  if (!showduino_mosfet_id_ok(id)) return false;
  if (!showduino_mosfet_id_equal(st->id, id) && st->id[0]) {
    /* Prefer matching id slot — caller should find by id. */
  }
  strncpy(st->id, id, sizeof(st->id) - 1);
  rest = colon + 1;
  int owned = 0, em = 0;
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(rest, "OWN=%d:EM=%d:O=%u,%u,%u,%u", &owned, &em, &a, &b, &c, &d) != 6) {
    return false;
  }
  if (a > 100 || b > 100 || c > 100 || d > 100) return false;
  st->haveLevels = true;
  st->owned = owned != 0;
  st->emergency = em != 0;
  st->levels[0] = (uint8_t)a;
  st->levels[1] = (uint8_t)b;
  st->levels[2] = (uint8_t)c;
  st->levels[3] = (uint8_t)d;
  if (em) strncpy(st->state, "EMERGENCY", sizeof(st->state) - 1);
  else if (owned) strncpy(st->state, "SHOW_CONTROLLED", sizeof(st->state) - 1);
  else strncpy(st->state, "SAFE", sizeof(st->state) - 1);
  return true;
}

void mosfetNodeLinkPublishToDirector() { publishState(true); }

uint8_t mosfetNodeLinkOnlineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].online) n++;
  }
  return n;
}

uint8_t mosfetNodeLinkSeenCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen) n++;
  }
  return n;
}

const MosfetNodeStatus *mosfetNodeLinkFind(const char *id) { return findById(id); }

const MosfetNodeStatus *mosfetNodeLinkSlot(uint8_t i) {
  if (i >= SHOWDUINO_MOSFET_NODE_MAX_NODES) return nullptr;
  if (!sNodes[i].used) return nullptr;
  return &sNodes[i];
}

void mosfetNodeLinkBegin() {
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) sNodes[i] = MosfetNodeStatus();
  sLastWire[0] = '\0';
  sLastDetail[0] = '\0';
}

void mosfetNodeLinkAllOff(const char *reason) {
  SD_LOGI("MOSFET", "ALL OFF (%s)", reason ? reason : "");
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].id[0]) continue;
    routeTo(sNodes[i].id, sSeq++, "MOSFET:ALL:OFF");
    zeroCachedLevels(&sNodes[i]);
  }
  publishState();
}

void mosfetNodeLinkLoop() {
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    MosfetNodeStatus &st = sNodes[i];
    if (!st.used || !st.online || !st.lastRxMs) continue;
    if ((millis() - st.lastRxMs) > 8000UL) {
      st.online = false;
      st.haveLevels = false;
      st.owned = false;
      zeroCachedLevels(&st);
      strncpy(st.state, "OFFLINE", sizeof(st.state) - 1);
      SD_LOGW("MOSFET", "MOSFET Node %s offline", st.id[0] ? st.id : "-");
      publishState();
    }
  }
  if ((millis() - sKeepaliveMs) >= 2000UL) {
    sKeepaliveMs = millis();
    for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online) continue;
      routeTo(sNodes[i].id, sSeq++, "MOSFET:OWN:GRANT");
      routeTo(sNodes[i].id, sSeq++, "MOSFET:STATUS");
    }
  }
  if ((millis() - sPublishMs) >= 3000UL) publishState(true);
}

bool mosfetNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:MOSFET:", 12) != 0) return false;
  const char *p = line + 12;
  SD_LOGT("MOSFET", "node RX %s", p);

  if (!strncmp(p, "ANNOUNCE:", 9)) {
    ShowduinoMosfetAnnounce an{};
    if (!showduino_mosfet_parse_announce(p, &an)) return true;
    MosfetNodeStatus *st = allocSlot(an.id, an.mac);
    if (!st) {
      SD_LOGW("MOSFET", "No slot for %s — max %u MOSFET Nodes",
              an.id[0] ? an.id : an.mac, (unsigned)SHOWDUINO_MOSFET_NODE_MAX_NODES);
      return true;
    }
    const bool wasOnline = st->online;
    st->seen = true;
    st->online = true;
    st->lastRxMs = millis();
    if (an.id[0]) strncpy(st->id, an.id, sizeof(st->id) - 1);
    if (an.name[0]) strncpy(st->name, an.name, sizeof(st->name) - 1);
    if (an.mac[0]) strncpy(st->mac, an.mac, sizeof(st->mac) - 1);
    if (an.firmware[0]) strncpy(st->firmware, an.firmware, sizeof(st->firmware) - 1);
    if (an.state[0]) strncpy(st->state, an.state, sizeof(st->state) - 1);
    if (!wasOnline) {
      SD_LOGI("MOSFET", "MOSFET Node online ID=%s FW=%s MAC=%s",
              st->id[0] ? st->id : "-",
              st->firmware[0] ? st->firmware : "-",
              st->mac[0] ? st->mac : "-");
      zeroCachedLevels(st);
    }
    if (st->id[0]) {
      routeTo(st->id, sSeq++, "MOSFET:OWN:GRANT");
      routeTo(st->id, sSeq++, "MOSFET:STATUS");
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "MOSFET:STATUS:", 14) || !strncmp(p, "STATUS:", 7)) {
    const char *status = p;
    if (!strncmp(p, "STATUS:", 7)) {
      /* rare short form — ignore if we can't match */
      status = nullptr;
    }
    MosfetNodeStatus *st = nullptr;
    if (status) {
      char id[SHOWDUINO_MOSFET_ID_MAX + 1] = "";
      const char *rest = status + 14;
      const char *colon = strchr(rest, ':');
      if (colon) {
        size_t n = (size_t)(colon - rest);
        if (n <= SHOWDUINO_MOSFET_ID_MAX) {
          memcpy(id, rest, n);
          id[n] = '\0';
          st = findById(id);
        }
      }
    }
    if (!st) {
      uint8_t nOn = 0;
      for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
        if (sNodes[i].used && sNodes[i].online) {
          nOn++;
          st = &sNodes[i];
        }
      }
      if (nOn != 1) st = nullptr;
    }
    if (st && status && parseStatusLine(status, st)) {
      st->online = true;
      st->lastRxMs = millis();
      publishState();
    } else if (st) {
      st->lastRxMs = millis();
    }
    return true;
  }

  if (!strncmp(p, "MOSFET:CAPS:", 12) || !strncmp(p, "CAPS:", 5)) {
    const char *caps = strchr(p, ':');
    if (caps) {
      caps = strchr(caps + 1, ':');
      if (caps) {
        uint8_t nOn = 0;
        MosfetNodeStatus *only = nullptr;
        for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
          if (sNodes[i].used && sNodes[i].online) {
            nOn++;
            only = &sNodes[i];
          }
        }
        if (nOn == 1 && only) {
          strncpy(only->capabilities, caps + 1, sizeof(only->capabilities) - 1);
          only->lastRxMs = millis();
        }
      }
    }
    return true;
  }

  if (!strncmp(p, "MOSFET:REJECTED:", 16) || !strncmp(p, "REJECTED:", 9)) {
    SD_LOGW("MOSFET", "%s", p);
    return true;
  }
  return true;
}

bool mosfetNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command || strncmp(command, "MOSFET:NODE:", 12) != 0) return false;
  if (reply && replyLen) reply[0] = '\0';

  char id[SHOWDUINO_MOSFET_ID_MAX + 1] = "";
  const char *inner = showduino_mosfet_strip_node_prefix(command, id, sizeof(id));
  if (!inner || !id[0]) {
    if (reply && replyLen) strncpy(reply, "REJECTED:MOSFET:BAD_ID", replyLen - 1);
    return true;
  }

  /* Build node-local command: ensure MOSFET: prefix */
  char local[SHOWDUINO_NODE_COMMAND_MAX];
  if (!strncmp(inner, "MOSFET:", 7)) {
    strncpy(local, inner, sizeof(local) - 1);
  } else {
    snprintf(local, sizeof(local), "MOSFET:%s", inner);
  }
  local[sizeof(local) - 1] = '\0';

  ShowduinoMosfetCmd cls = showduino_mosfet_classify_command(local);
  const bool alwaysOk =
      cls == SHOWDUINO_MOSFET_CMD_STATUS ||
      cls == SHOWDUINO_MOSFET_CMD_CAPS ||
      cls == SHOWDUINO_MOSFET_CMD_ALL_OFF ||
      cls == SHOWDUINO_MOSFET_CMD_OUT_OFF;
  if (emergencyLocked && showduino_mosfet_cmd_theatrical(cls)) {
    if (reply && replyLen) strncpy(reply, "REJECTED:MOSFET:EMERGENCY_ACTIVE", replyLen - 1);
    return true;
  }

  MosfetNodeStatus *st = findById(id);
  if ((!st || !st->online) && !alwaysOk) {
    SD_LOGW("MOSFET", "NODE_UNAVAILABLE %s — timeline continues", id);
    if (reply && replyLen) {
      snprintf(reply, replyLen, "REJECTED:MOSFET:OFFLINE:%s", id);
    }
    return true;
  }

  const uint32_t seq = sSeq++;
  if (st) st->lastSeq = seq;
  routeTo(id, seq, local);
  if (reply && replyLen) {
    snprintf(reply, replyLen, "MOSFET:PENDING:%s:%lu", id, (unsigned long)seq);
  }
  return true;
}

void mosfetNodeLinkOnEmergency(bool active) {
  const char *cmd = active ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR";
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].id[0]) continue;
    routeTo(sNodes[i].id, 0, cmd);
    if (active) {
      zeroCachedLevels(&sNodes[i]);
      sNodes[i].emergency = true;
      strncpy(sNodes[i].state, "EMERGENCY", sizeof(sNodes[i].state) - 1);
    } else {
      sNodes[i].emergency = false;
      zeroCachedLevels(&sNodes[i]);
      /* Remain all off after clear — do not restore. */
      if (sNodes[i].online) {
        strncpy(sNodes[i].state, "SAFE", sizeof(sNodes[i].state) - 1);
      }
    }
  }
  if (active) {
    /* Belt-and-braces explicit ALL OFF */
    for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].id[0]) continue;
      routeTo(sNodes[i].id, sSeq++, "MOSFET:ALL:OFF");
    }
  }
  publishState();
}

void mosfetNodeLinkAppendJsonArray(String &json) {
  json += "[\n";
  bool first = true;
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    const MosfetNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    const uint32_t age = (st.lastRxMs && st.online) ? (millis() - st.lastRxMs) : 0;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"firmware\": \"";
    json += st.firmware;
    json += "\",\n      \"capabilities\": \"";
    json += st.capabilities[0] ? st.capabilities : SHOWDUINO_MOSFET_CAPS;
    json += "\",\n      \"owned\": ";
    json += st.owned ? "true" : "false";
    json += ",\n      \"emergency\": ";
    json += st.emergency ? "true" : "false";
    json += ",\n      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"lastContactMs\": ";
    json += String((unsigned long)age);
    json += ",\n      \"outputs\": [\n";
    for (uint8_t o = 0; o < SHOWDUINO_MOSFET_OUT_COUNT; ++o) {
      if (o) json += ",\n";
      json += "        {\"id\":\"out";
      json += String((unsigned)(o + 1));
      json += "\",\"name\":\"";
      json += st.outNames[o];
      json += "\",\"level\":";
      json += String((unsigned)st.levels[o]);
      json += ",\"active\":";
      json += st.levels[o] ? "true" : "false";
      json += "}";
    }
    json += "\n      ]\n    }";
  }
  json += "\n  ]";
}

void mosfetNodeLinkAppendDevicesJson(String &json, bool &first) {
  static const int kPins[4] = {
    SHOWDUINO_MOSFET_OUT1_GPIO_DEFAULT,
    SHOWDUINO_MOSFET_OUT2_GPIO_DEFAULT,
    SHOWDUINO_MOSFET_OUT3_GPIO_DEFAULT,
    SHOWDUINO_MOSFET_OUT4_GPIO_DEFAULT
  };
  for (uint8_t i = 0; i < SHOWDUINO_MOSFET_NODE_MAX_NODES; ++i) {
    const MosfetNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"friendlyName\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"board\": \"ESP32_MOS_X4\",\n";
    json += "      \"role\": \"MOSFET\",\n";
    json += "      \"type\": \"MOSFET\",\n";
    json += "      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"espnow-node\",\n";
    json += "      \"connection\": \"ESP-NOW Node\",\n";
    json += "      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmwareVersion\": \"";
    json += st.firmware;
    json += "\",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"capabilities\": [\"DIGITAL\",\"PWM\",\"PULSE\",\"FADE\",\"ALL_OFF\"],\n";
    json += "      \"outputs\": [\n";
    for (uint8_t o = 0; o < SHOWDUINO_MOSFET_OUT_COUNT; ++o) {
      if (o) json += ",\n";
      json += "        {\"id\":\"out";
      json += String((unsigned)(o + 1));
      json += "\",\"kind\":\"mosfet\",\"label\":\"";
      json += st.outNames[o];
      json += "\",\"channel\":";
      json += String((unsigned)(o + 1));
      json += ",\"pin\":";
      json += String(kPins[o]);
      json += ",\"level\":";
      json += String((unsigned)st.levels[o]);
      json += ",\"active\":";
      json += st.levels[o] ? "true" : "false";
      json += ",\"pwm\":true}";
    }
    json += "\n      ]\n    }";
  }
}
