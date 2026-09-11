#include "PixelNodeLink.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

extern bool emergencyLocked;
bool sendToDirector(const String &message);
void stageCommsSendLine(const char *line);

static PixelNodeStatus sNodes[SHOWDUINO_PIXEL_NODE_MAX_NODES];
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";

static void routeTo(const char *id, uint32_t seq, const char *cmd) {
  char line[180];
  snprintf(line, sizeof(line), "ROUTE:PIXEL:%s:%lu:%s",
           id && id[0] ? id : "LED-00",
           (unsigned long)seq,
           cmd ? cmd : "");
  stageCommsSendLine(line);
}

static PixelNodeStatus *findById(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && showduino_pixel_id_equal(sNodes[i].id, id)) return &sNodes[i];
  }
  return nullptr;
}

static PixelNodeStatus *findByMac(const char *mac) {
  if (!mac || strlen(mac) < 17) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && !strcasecmp(sNodes[i].mac, mac)) return &sNodes[i];
  }
  return nullptr;
}

static PixelNodeStatus *allocSlot(const char *id, const char *mac) {
  PixelNodeStatus *hit = findById(id);
  if (!hit) hit = findByMac(mac);
  if (hit) return hit;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used) {
      sNodes[i] = PixelNodeStatus();
      sNodes[i].used = true;
      return &sNodes[i];
    }
  }
  return nullptr;
}

static const char *wireToken() {
  uint8_t online = 0, seen = 0, fault = 0, em = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) online++;
    if (!strcmp(sNodes[i].state, "FAULT")) fault++;
    if (!strcmp(sNodes[i].state, "EMERGENCY")) em++;
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
    snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_PIXEL_PREFIX, tok);
    if (sendToDirector(String(line))) {
      strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
      sLastWire[sizeof(sLastWire) - 1] = '\0';
    }
  }
  const char *firstId = "-";
  const char *firstSt = "OFFLINE";
  uint8_t online = 0, seen = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) {
      if (!online) {
        firstId = sNodes[i].id[0] ? sNodes[i].id : "-";
        firstSt = sNodes[i].state[0] ? sNodes[i].state : "ONLINE";
      }
      online++;
    }
  }
  char detail[96];
  snprintf(detail, sizeof(detail), "%s%u:%u:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_PIXEL_DETAIL_PREFIX,
           (unsigned)online, (unsigned)seen, firstId, firstSt);
  if (force || strcmp(sLastDetail, detail) != 0) {
    if (sendToDirector(String(detail))) {
      strncpy(sLastDetail, detail, sizeof(sLastDetail) - 1);
    }
  }
  sPublishMs = millis();
}

void pixelNodeLinkPublishToDirector() { publishState(true); }

uint8_t pixelNodeLinkOnlineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].online) n++;
  }
  return n;
}

uint8_t pixelNodeLinkSeenCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen) n++;
  }
  return n;
}

const PixelNodeStatus *pixelNodeLinkFind(const char *id) { return findById(id); }

void pixelNodeLinkBegin() {
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) sNodes[i] = PixelNodeStatus();
  sLastWire[0] = '\0';
  sLastDetail[0] = '\0';
}

void pixelNodeLinkLoop() {
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    PixelNodeStatus &st = sNodes[i];
    if (!st.used || !st.online || !st.lastRxMs) continue;
    if ((millis() - st.lastRxMs) > 8000UL) {
      st.online = false;
      strncpy(st.state, "OFFLINE", sizeof(st.state) - 1);
      st.pending = false;
      SD_LOGW("PIXEL", "Pixel Node %s offline", st.id[0] ? st.id : "-");
      publishState();
    }
  }
  if ((millis() - sKeepaliveMs) >= 2000UL) {
    sKeepaliveMs = millis();
    for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online) continue;
      routeTo(sNodes[i].id, sSeq++, "PIXEL:OWN:GRANT");
      routeTo(sNodes[i].id, sSeq++, "PIXEL:STATUS");
    }
  }
  if ((millis() - sPublishMs) >= 3000UL) publishState(true);
}

bool pixelNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:PIXEL:", 11) != 0) return false;
  const char *p = line + 11;
  SD_LOGT("PIXEL", "node RX %s", p);

  if (!strncmp(p, "ANNOUNCE:", 9)) {
    ShowduinoPixelAnnounce an{};
    if (!showduino_pixel_parse_announce(p, &an)) return true;
    PixelNodeStatus *st = allocSlot(an.id, an.mac);
    if (!st) {
      SD_LOGW("PIXEL", "No slot for %s — max %u Pixel Nodes",
              an.id[0] ? an.id : an.mac, (unsigned)SHOWDUINO_PIXEL_NODE_MAX_NODES);
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
    st->pixelCount = an.pixelCount;
    st->initialised = an.initialised != 0;
    st->segments = an.segments;
    if (!wasOnline) {
      SD_LOGI("PIXEL", "Pixel Node online ID=%s FW=%s MAC=%s",
              st->id[0] ? st->id : "-",
              st->firmware[0] ? st->firmware : "-",
              st->mac[0] ? st->mac : "-");
    }
    if (st->id[0]) {
      routeTo(st->id, sSeq++, "PIXEL:OWN:GRANT");
      routeTo(st->id, sSeq++, "PIXEL:STATUS");
    }
    publishState();
    return true;
  }

  /* STATUS / PIXEL:* reports without announce still refresh last-seen if we can
   * match a single online node; otherwise keep the report as TRACE. */
  PixelNodeStatus *only = nullptr;
  uint8_t nOn = 0;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].online) {
      nOn++;
      only = &sNodes[i];
    }
  }
  if (nOn == 1 && only) {
    only->lastRxMs = millis();
  }

  if (!strncmp(p, "STATUS:", 7)) {
    if (nOn == 1 && only) {
      char buf[96];
      strncpy(buf, p + 7, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      char *save = nullptr;
      char *stt = strtok_r(buf, ":", &save);
      char *pc = strtok_r(nullptr, ":", &save);
      char *ini = strtok_r(nullptr, ":", &save);
      char *sg = strtok_r(nullptr, ":", &save);
      char *fault = strtok_r(nullptr, ":", &save);
      if (stt) strncpy(only->state, stt, sizeof(only->state) - 1);
      if (pc && pc[0] == 'P') only->pixelCount = (uint16_t)atoi(pc + 1);
      if (ini && ini[0] == 'I') only->initialised = ini[1] == '1';
      if (sg && sg[0] == 'S') only->segments = (uint8_t)atoi(sg + 1);
      if (fault && strcmp(fault, "-") != 0 && strcmp(fault, "NONE") != 0) {
        strncpy(only->lastError, fault, sizeof(only->lastError) - 1);
      } else if (fault) {
        only->lastError[0] = '\0';
      }
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "PIXEL:OWNED:", 12) || !strncmp(p, "OWNED:", 6)) return true;
  if (!strncmp(p, "PIXEL:EMERGENCY:", 16) || !strncmp(p, "EMERGENCY:", 10)) {
    if (nOn == 1 && only) strncpy(only->state, "EMERGENCY", sizeof(only->state) - 1);
    SD_LOG_EMERGENCY("PIXEL", true);
    publishState();
    return true;
  }
  if (!strncmp(p, "PIXEL:FAILED:", 13) || !strncmp(p, "FAILED:", 7)) {
    if (nOn == 1 && only) {
      const char *reason = strrchr(p, ':');
      if (reason && reason[1]) strncpy(only->lastError, reason + 1, sizeof(only->lastError) - 1);
      only->pending = false;
    }
    publishState();
    return true;
  }
  return true;
}

bool pixelNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command || strncmp(command, "PIXEL:NODE:", 11) != 0) return false;
  if (reply && replyLen) reply[0] = '\0';

  char id[SHOWDUINO_PIXEL_ID_MAX + 1] = "";
  char inner[SHOWDUINO_NODE_COMMAND_MAX];
  if (!showduino_pixel_inner_command(command, id, sizeof(id), inner, sizeof(inner))) {
    if (reply && replyLen) strncpy(reply, "REJECTED:PIXEL:BAD_ID", replyLen - 1);
    return true;
  }

  const bool alwaysOk =
      !strcmp(inner, "PIXEL:STATUS") ||
      !strcmp(inner, "PIXEL:OFF") ||
      !strcmp(inner, "PIXEL:BLACKOUT");
  if (emergencyLocked && !alwaysOk) {
    if (reply && replyLen) strncpy(reply, "REJECTED:PIXEL:EMERGENCY_ACTIVE", replyLen - 1);
    return true;
  }

  PixelNodeStatus *st = findById(id);
  if ((!st || !st->online) && !alwaysOk) {
    SD_LOGW("PIXEL", "NODE_UNAVAILABLE %s — timeline continues", id);
    sendToDirector(String(SHOWDUINO_WIRE_NODE_UNAVAILABLE_PREFIX) + "PIXEL:" + id);
    if (reply && replyLen) {
      showduino_pixel_format_offline(reply, replyLen, id);
    }
    return true;
  }

  const uint32_t seq = sSeq++;
  if (st) {
    st->pending = true;
    st->pendingSeq = seq;
    strncpy(st->lastLife, "PENDING", sizeof(st->lastLife) - 1);
  }
  routeTo(id, seq, inner);
  if (reply && replyLen) {
    snprintf(reply, replyLen, "PIXEL:PENDING:%s:%lu", id, (unsigned long)seq);
  }
  return true;
}

void pixelNodeLinkOnEmergency(bool active) {
  const char *cmd = active ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR";
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].id[0]) continue;
    routeTo(sNodes[i].id, 0, cmd);
    if (active && sNodes[i].online) {
      strncpy(sNodes[i].state, "EMERGENCY", sizeof(sNodes[i].state) - 1);
      sNodes[i].pending = false;
    }
  }
  publishState();
}

void pixelNodeLinkAppendJsonArray(String &json) {
  json += "[\n";
  bool first = true;
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    const PixelNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    const uint32_t age = (st.lastRxMs && st.online) ? (millis() - st.lastRxMs) : 0;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"type\": \"PIXEL\",\n";
    json += "      \"seen\": true,\n      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"initialised\": ";
    json += st.initialised ? "true" : "false";
    json += ",\n      \"pixelCount\": ";
    json += String((unsigned)st.pixelCount);
    json += ",\n      \"maxPixels\": ";
    json += String((unsigned)SHOWDUINO_PIXEL_NODE_MAX_PIXELS);
    json += ",\n      \"segments\": ";
    json += String((unsigned)st.segments);
    json += ",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmware\": \"";
    json += st.firmware;
    json += "\",\n      \"lastError\": \"";
    json += st.lastError;
    json += "\",\n      \"lastContactMs\": ";
    json += String((unsigned long)age);
    json += "\n    }";
  }
  json += "\n  ]";
}

void pixelNodeLinkAppendDevicesJson(String &json, bool &first) {
  for (uint8_t i = 0; i < SHOWDUINO_PIXEL_NODE_MAX_NODES; ++i) {
    const PixelNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"Pixel Node\",\n";
    json += "      \"friendlyName\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"board\": \"ESP32-C3\",\n";
    json += "      \"role\": \"PIXEL\",\n";
    json += "      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"espnow-node\",\n";
    json += "      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmwareVersion\": \"";
    json += st.firmware;
    json += "\",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"pixelCount\": ";
    json += String((unsigned)st.pixelCount);
    json += ",\n      \"initialised\": ";
    json += st.initialised ? "true" : "false";
    json += "\n    }";
  }
}
