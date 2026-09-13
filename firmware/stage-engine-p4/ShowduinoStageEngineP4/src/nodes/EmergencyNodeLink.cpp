#include "EmergencyNodeLink.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

extern bool emergencyLocked;
extern uint8_t gEmergencySourceId;
void triggerEmergencyWireless();
void stageCommsSendLine(const char *line);
bool sendToDirector(const String &message);

/* Wireless emergency source id used by the P4 latch. Must match .ino. */
#ifndef SHOWDUINO_EMERGENCY_SOURCE_WIRELESS
#define SHOWDUINO_EMERGENCY_SOURCE_WIRELESS 4
#endif

static EmergencyNodeStatus sNodes[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
static ShowduinoEmergencyAuthority sAuth;
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static uint32_t sDupLogMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";
static char sLastSafety[32] = "";
static char sLastSource[64] = "";

static void routeTo(const char *id, uint32_t seq, const char *cmd) {
  char line[180];
  snprintf(line, sizeof(line), "ROUTE:EMERGENCY:%s:%lu:%s",
           id && id[0] ? id : SHOWDUINO_EMERGENCY_ID_DEFAULT,
           (unsigned long)seq,
           cmd ? cmd : "");
  stageCommsSendLine(line);
}

static EmergencyNodeStatus *findById(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && showduino_emergency_id_equal(sNodes[i].id, id)) {
      return &sNodes[i];
    }
  }
  return nullptr;
}

static EmergencyNodeStatus *findByMac(const char *mac) {
  if (!mac || strlen(mac) < 17) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && !strcasecmp(sNodes[i].mac, mac)) return &sNodes[i];
  }
  return nullptr;
}

static EmergencyNodeStatus *allocSlot(const char *id, const char *mac) {
  EmergencyNodeStatus *hit = findById(id);
  if (!hit) hit = findByMac(mac);
  if (hit) return hit;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used) {
      sNodes[i] = EmergencyNodeStatus();
      sNodes[i].used = true;
      return &sNodes[i];
    }
  }
  return nullptr;
}

static void publishSafety() {
  const bool fault = emergencyNodeLinkSafetyFault();
  showduino_emergency_authority_offline(&sAuth, fault ? 1 : 0);
  char line[40];
  snprintf(line, sizeof(line), "STATE:SAFETY:%s",
           fault ? SHOWDUINO_WIRE_SAFETY_ESTOP_FAULT : SHOWDUINO_WIRE_SAFETY_ESTOP_OK);
  if (strcmp(sLastSafety, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastSafety, line, sizeof(sLastSafety) - 1);
    }
    if (fault) {
      SD_LOGW("ESTOP", "SAFETY WARNING — Emergency Node offline");
    }
  }
}

static void publishSource() {
  if (!emergencyLocked) {
    if (sLastSource[0]) sLastSource[0] = 0;
    return;
  }
  char line[80];
  if (gEmergencySourceId == 2) {
    snprintf(line, sizeof(line), "%sHARDWIRED",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else if (gEmergencySourceId == SHOWDUINO_EMERGENCY_SOURCE_WIRELESS &&
             sAuth.primary_id[0]) {
    snprintf(line, sizeof(line), "%sWIRELESS:%s:%s",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX,
             sAuth.primary_id,
             sAuth.primary_name[0] ? sAuth.primary_name : "-");
  } else if (gEmergencySourceId == 3) {
    snprintf(line, sizeof(line), "%sUSB",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else if (gEmergencySourceId == 1) {
    snprintf(line, sizeof(line), "%sREMOTE",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else {
    return;
  }
  if (strcmp(sLastSource, line) != 0 && sendToDirector(String(line))) {
    strncpy(sLastSource, line, sizeof(sLastSource) - 1);
  }
}

static const char *wireToken() {
  uint8_t online = 0, seen = 0, asserting = 0, offline = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) online++;
    else offline++;
    if (sNodes[i].latched) asserting++;
  }
  if (asserting) return "ACTIVE";
  if (offline) return "FAULT";
  if (!seen || !online) return "OFFLINE";
  return "ONLINE";
}

static void publishState(bool force = false) {
  char line[48];
  const char *tok = wireToken();
  if (force || strcmp(sLastWire, tok) != 0) {
    snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_PREFIX, tok);
    if (sendToDirector(String(line))) {
      strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
    }
  }

  const char *firstId = "-";
  const char *firstName = "-";
  const char *firstSt = "OFFLINE";
  uint8_t online = 0, seen = 0, asserting = 0, offline = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].latched) asserting++;
    if (sNodes[i].online) {
      if (!online) {
        firstId = sNodes[i].id[0] ? sNodes[i].id : "-";
        firstName = sNodes[i].name[0] ? sNodes[i].name : "-";
        firstSt = sNodes[i].state[0] ? sNodes[i].state : "ONLINE";
      }
      online++;
    } else {
      offline++;
    }
  }
  char detail[96];
  snprintf(detail, sizeof(detail), "%s%u:%u:%u:%u:%s:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX,
           (unsigned)online, (unsigned)seen, (unsigned)asserting, (unsigned)offline,
           firstId, firstName, firstSt);
  if (force || strcmp(sLastDetail, detail) != 0) {
    if (sendToDirector(String(detail))) {
      strncpy(sLastDetail, detail, sizeof(sLastDetail) - 1);
    }
  }

  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    char stline[96];
    snprintf(stline, sizeof(stline), "%s%u:%s:%s:%u:%u:%u:%u:%s",
             SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX,
             (unsigned)i,
             sNodes[i].id[0] ? sNodes[i].id : "-",
             sNodes[i].name[0] ? sNodes[i].name : "-",
             sNodes[i].online ? 1U : 0U,
             sNodes[i].inputOpen ? 1U : 0U,
             sNodes[i].latched ? 1U : 0U,
             sNodes[i].acked ? 1U : 0U,
             sNodes[i].state[0] ? sNodes[i].state : "OFFLINE");
    sendToDirector(String(stline));
  }
  publishSafety();
  publishSource();
  sPublishMs = millis();
}

void emergencyNodeLinkPublishToDirector() { publishState(true); }

uint8_t emergencyNodeLinkOnlineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].online) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkSeenCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkOfflineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen && !sNodes[i].online) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkAssertingCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].latched) n++;
  }
  return n;
}

bool emergencyNodeLinkSafetyFault() {
  return emergencyNodeLinkOfflineCount() > 0;
}

const EmergencyNodeStatus *emergencyNodeLinkFind(const char *id) { return findById(id); }

void emergencyNodeLinkPrimarySource(char *kind, size_t kindn,
                                    char *id, size_t idn,
                                    char *name, size_t namen) {
  if (kind && kindn) {
    strncpy(kind, sAuth.primary_kind[0] ? sAuth.primary_kind : "", kindn - 1);
    kind[kindn - 1] = 0;
  }
  if (id && idn) {
    strncpy(id, sAuth.primary_id, idn - 1);
    id[idn - 1] = 0;
  }
  if (name && namen) {
    strncpy(name, sAuth.primary_name, namen - 1);
    name[namen - 1] = 0;
  }
}

static void applyAssert(const char *id, const char *name) {
  if (!showduino_emergency_id_ok(id)) return;
  EmergencyNodeStatus *st = allocSlot(id, nullptr);
  if (st) {
    st->seen = true;
    st->online = true;
    st->lastRxMs = millis();
    st->latched = true;
    strncpy(st->id, id, sizeof(st->id) - 1);
    if (name && name[0]) strncpy(st->name, name, sizeof(st->name) - 1);
    strncpy(st->state, "LATCHED", sizeof(st->state) - 1);
  }
  const int first = showduino_emergency_authority_assert(&sAuth, id, name);
  if (first && !emergencyLocked) {
    SD_LOGI("ESTOP", "EMERGENCY ASSERT SOURCE=%s NAME=%s",
            id, (name && name[0]) ? name : "-");
    triggerEmergencyWireless();
  } else {
    if (showduino_log_rate_ok(&sDupLogMs, millis(), 2000UL)) {
      SD_LOGI("ESTOP", "EMERGENCY DUPLICATE SOURCE=%s", id);
    }
  }
  if (st && st->id[0]) {
    routeTo(st->id, sSeq++, SHOWDUINO_ESTOP_ACK_LATCHED);
    st->acked = true;
  }
  publishState(true);
}

void emergencyNodeLinkBegin() {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    sNodes[i] = EmergencyNodeStatus();
  }
  showduino_emergency_authority_init(&sAuth);
  sLastWire[0] = 0;
  sLastDetail[0] = 0;
  sLastSafety[0] = 0;
  sLastSource[0] = 0;
}

void emergencyNodeLinkLoop() {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.online || !st.lastRxMs) continue;
    if ((millis() - st.lastRxMs) > SHOWDUINO_EMERGENCY_COMMS_TIMEOUT_MS) {
      st.online = false;
      strncpy(st.state, "OFFLINE", sizeof(st.state) - 1);
      SD_LOGW("ESTOP", "EMERGENCY NODE OFFLINE %s %s",
              st.id[0] ? st.id : "-",
              st.name[0] ? st.name : "-");
      publishState(true);
    }
  }
  if ((millis() - sKeepaliveMs) >= SHOWDUINO_EMERGENCY_HEARTBEAT_MS) {
    sKeepaliveMs = millis();
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online) continue;
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_OWN_GRANT);
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_STATUS);
    }
  }
  if ((millis() - sPublishMs) >= 3000UL) publishState(true);
}

bool emergencyNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:EMERGENCY:", 15) != 0) return false;
  const char *p = line + 15;

  if (showduino_emergency_is_clear_token(p) &&
      strcmp(p, "EMERGENCY:CLEAR") != 0) {
    SD_LOGW("ESTOP", "Rejected node clear token");
    (void)showduino_emergency_authority_apply_node_clear(&sAuth, p);
    return true;
  }

  char assertId[16] = "";
  char assertName[24] = "";
  if (showduino_emergency_parse_assert(p, assertId, sizeof(assertId),
                                       assertName, sizeof(assertName))) {
    applyAssert(assertId, assertName);
    return true;
  }

  if (!strncmp(p, "ANNOUNCE:", 9)) {
    ShowduinoEmergencyAnnounce an{};
    if (!showduino_emergency_parse_announce(p, &an)) return true;
    EmergencyNodeStatus *st = allocSlot(an.id, an.mac);
    if (!st) {
      SD_LOGW("ESTOP", "No slot for %s — max %u Emergency Nodes",
              an.id[0] ? an.id : an.mac,
              (unsigned)SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
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
    st->inputOpen = an.input_open != 0;
    st->latched = an.latched != 0;
    st->acked = an.acked != 0;
    if (!wasOnline) {
      SD_LOGI("ESTOP", "EMERGENCY NODE ONLINE %s %s",
              st->id[0] ? st->id : "-",
              st->name[0] ? st->name : "-");
    }
    if (st->latched && st->id[0]) {
      applyAssert(st->id, st->name);
    } else if (st->id[0]) {
      routeTo(st->id, sSeq++, SHOWDUINO_ESTOP_OWN_GRANT);
      routeTo(st->id, sSeq++, SHOWDUINO_ESTOP_STATUS);
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "STATUS:", 7)) {
    EmergencyNodeStatus *only = nullptr;
    uint8_t nOn = 0;
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (sNodes[i].used && sNodes[i].online) {
        nOn++;
        only = &sNodes[i];
      }
    }
    if (nOn == 1 && only) only->lastRxMs = millis();
    if (strstr(p, "L=1") && only && only->id[0]) {
      applyAssert(only->id, only->name);
    }
    publishState();
    return true;
  }

  return true;
}

bool emergencyNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command) return false;
  if (showduino_emergency_is_clear_token(command) &&
      strncmp(command, "EMERGENCY:CLEAR", 15) != 0) {
    if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NO_CLEAR", replyLen - 1);
    return true;
  }
  if (!strcmp(command, "ESTOP:STATUS") || !strncmp(command, "ESTOP:NODE:", 11) ||
      !strncmp(command, "EMERGENCY:NODE:", 15)) {
    emergencyNodeLinkPublishToDirector();
    if (reply && replyLen) strncpy(reply, "OK:ESTOP:STATUS", replyLen - 1);
    return true;
  }
  return false;
}

void emergencyNodeLinkOnEmergency(bool active) {
  if (!active) {
    showduino_emergency_authority_legitimate_clear(&sAuth);
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].id[0]) continue;
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_GLOBAL_OBSERVED);
      routeTo(sNodes[i].id, sSeq++, "EMERGENCY:CLEAR");
      sNodes[i].acked = false;
    }
    sLastSource[0] = 0;
  } else {
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online || !sNodes[i].latched) continue;
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_ACK_LATCHED);
      sNodes[i].acked = true;
    }
  }
  publishState(true);
}

void emergencyNodeLinkAppendJsonArray(String &json) {
  json += "[\n";
  bool first = true;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    const EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    const uint32_t age = (st.lastRxMs && st.online) ? (millis() - st.lastRxMs) : 0;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"type\": \"EMERGENCY\",\n";
    json += "      \"seen\": true,\n      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"input\": \"";
    json += st.inputOpen ? "OPEN" : "CLOSED";
    json += "\",\n      \"latch\": \"";
    json += st.latched ? "LATCHED" : "CLEAR";
    json += "\",\n      \"acked\": ";
    json += st.acked ? "true" : "false";
    json += ",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmware\": \"";
    json += st.firmware;
    json += "\",\n      \"lastContactMs\": ";
    json += String((unsigned long)age);
    json += "\n    }";
  }
  json += "\n  ]";
}

void emergencyNodeLinkAppendDevicesJson(String &json, bool &first) {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    const EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"Emergency Node\",\n";
    json += "      \"friendlyName\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"board\": \"ESP32-C3\",\n";
    json += "      \"role\": \"EMERGENCY\",\n";
    json += "      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"espnow-node\",\n";
    json += "      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmwareVersion\": \"";
    json += st.firmware;
    json += "\",\n      \"state\": \"";
    json += st.state;
    json += "\"\n    }";
  }
}
