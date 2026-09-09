#include "LampNodeLink.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"

extern bool emergencyLocked;
bool sendToDirector(const String &message);
void stageCommsSendLine(const char *line);

static LampNodeStatus sSt;
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";

static void route(uint32_t seq, const char *cmd) {
  char line[180];
  snprintf(line, sizeof(line), "ROUTE:LAMP:%lu:%s", (unsigned long)seq, cmd ? cmd : "");
  stageCommsSendLine(line);
}

static const char *wireToken() {
  if (!sSt.online) return "OFFLINE";
  if (!strcmp(sSt.state, "EMERGENCY")) return "EMERGENCY";
  if (!strcmp(sSt.state, "FAULT")) return "FAULT";
  if (sSt.fxActive) return "ACTIVE";
  return "ONLINE";
}

static void publishExtra(bool force) {
  char line[96];
  snprintf(line, sizeof(line), "%s%s:%s:%u:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_LAMP_DETAIL_PREFIX,
           sSt.state[0] ? sSt.state : "OFFLINE",
           sSt.fx[0] ? sSt.fx : "-",
           (unsigned)sSt.brightness,
           sSt.mac[0] ? sSt.mac : "-",
           sSt.firmware[0] ? sSt.firmware : "-");
  if (force || strcmp(sLastDetail, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastDetail, line, sizeof(sLastDetail) - 1);
    }
  }
}

static void publishState(bool force = false) {
  char line[48];
  const char *tok = wireToken();
  if (!force && !strcmp(sLastWire, tok)) return;
  snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_LAMP_PREFIX, tok);
  if (!sendToDirector(String(line))) return;
  strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
  sLastWire[sizeof(sLastWire) - 1] = '\0';
  sPublishMs = millis();
  publishExtra(force);
}

void lampNodeLinkPublishToDirector() {
  publishState(true);
}

static void markRx() {
  sSt.seen = true;
  sSt.online = true;
  sSt.lastRxMs = millis();
}

void lampNodeLinkBegin() {
  sSt = LampNodeStatus();
  sLastWire[0] = '\0';
  sLastDetail[0] = '\0';
}

void lampNodeLinkLoop() {
  if (sSt.online && sSt.lastRxMs && (millis() - sSt.lastRxMs) > 8000UL) {
    sSt.online = false;
    strncpy(sSt.state, "OFFLINE", sizeof(sSt.state) - 1);
    sSt.fxActive = false;
    sSt.pending = false;
    SD_LOGW("LAMP", "Lamp Node offline");
    publishState();
  }
  if (sSt.online && (millis() - sKeepaliveMs) >= 2000UL) {
    sKeepaliveMs = millis();
    route(sSeq++, "LAMP:NODE:OWN:GRANT");
    route(sSeq++, "LAMP:STATUS");
  }
  if ((millis() - sPublishMs) >= 3000UL) {
    publishState(true);
  }
}

bool lampNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:LAMP:", 10) != 0) return false;
  const char *p = line + 10;
  const bool wasOnline = sSt.online;
  markRx();
  SD_LOGT("LAMP", "node RX %s", p);

  if (!strncmp(p, "ANNOUNCE:", 9)) {
    p += 9;
    if (strlen(p) >= 17) {
      memcpy(sSt.mac, p, 17);
      sSt.mac[17] = '\0';
      p += 17;
      if (*p == ':') p++;
      const char *colon = strchr(p, ':');
      if (colon) {
        size_t n = (size_t)(colon - p);
        if (n >= sizeof(sSt.firmware)) n = sizeof(sSt.firmware) - 1;
        memcpy(sSt.firmware, p, n);
        sSt.firmware[n] = '\0';
        strncpy(sSt.state, colon + 1, sizeof(sSt.state) - 1);
      }
    }
    if (!wasOnline) {
      SD_LOGI("LAMP", "Lamp Node online FW=%s MAC=%s",
              sSt.firmware[0] ? sSt.firmware : "-",
              sSt.mac[0] ? sSt.mac : "-");
    }
    route(sSeq++, "LAMP:NODE:OWN:GRANT");
    route(sSeq++, "LAMP:STATUS");
    publishState();
    return true;
  }

  if (!strncmp(p, "STATUS:", 7)) {
    char buf[96];
    strncpy(buf, p + 7, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    char *st = strtok_r(buf, ":", &save);
    char *bri = strtok_r(nullptr, ":", &save);
    char *fx = strtok_r(nullptr, ":", &save);
    char *fault = strtok_r(nullptr, ":", &save);
    char prev[20];
    strncpy(prev, sSt.state, sizeof(prev) - 1);
    prev[sizeof(prev) - 1] = '\0';
    if (st) strncpy(sSt.state, st, sizeof(sSt.state) - 1);
    if (bri && bri[0] == 'B') sSt.brightness = (uint8_t)atoi(bri + 1);
    if (fx && strcmp(fx, "-") != 0) {
      strncpy(sSt.fx, fx, sizeof(sSt.fx) - 1);
      sSt.fxActive = true;
    } else if (fx) {
      sSt.fx[0] = '\0';
      sSt.fxActive = false;
    }
    if (fault && strcmp(fault, "-") != 0 && strcmp(fault, "NONE") != 0) {
      strncpy(sSt.lastError, fault, sizeof(sSt.lastError) - 1);
    } else {
      sSt.lastError[0] = '\0';
    }
    if (st && strcmp(prev, sSt.state) != 0) {
      SD_LOGI("LAMP", "State -> %s", sSt.state);
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "LAMP:OWNED:", 11) || !strncmp(p, "OWNED:", 6)) {
    SD_LOGT("LAMP", "%s", p);
    return true;
  }

  if (!strncmp(p, "LAMP:STARTED:", 13) || !strncmp(p, "STARTED:", 8)) {
    const char *fx = strchr(p, ':');
    if (fx) fx = strchr(fx + 1, ':');
    if (fx && fx[1]) strncpy(sSt.fx, fx + 1, sizeof(sSt.fx) - 1);
    sSt.fxActive = true;
    strncpy(sSt.lastLife, "STARTED", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    SD_LOGI("LAMP", "FX: %s", sSt.fx[0] ? sSt.fx : "-");
    publishState();
    return true;
  }

  if (!strncmp(p, "LAMP:IDLE:", 10) || !strncmp(p, "IDLE:", 5) ||
      !strncmp(p, "LAMP:STOPPED:", 13) || !strncmp(p, "STOPPED:", 8)) {
    sSt.fxActive = false;
    sSt.fx[0] = '\0';
    strncpy(sSt.lastLife, "IDLE", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    SD_LOGI("LAMP", "FX stopped");
    publishState();
    return true;
  }

  if (!strncmp(p, "LAMP:EMERGENCY:", 15) || !strncmp(p, "EMERGENCY:", 10)) {
    strncpy(sSt.state, "EMERGENCY", sizeof(sSt.state) - 1);
    sSt.fxActive = false;
    sSt.pending = false;
    SD_LOG_EMERGENCY("LAMP", true);
    publishState();
    return true;
  }

  if (!strncmp(p, "LAMP:FAILED:", 12) || !strncmp(p, "FAILED:", 7)) {
    const char *reason = strrchr(p, ':');
    if (reason && reason[1]) strncpy(sSt.lastError, reason + 1, sizeof(sSt.lastError) - 1);
    sSt.pending = false;
    SD_LOGW("LAMP", "Failed: %s", sSt.lastError[0] ? sSt.lastError : "-");
    publishState();
    return true;
  }

  return true;
}

bool lampNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command) return false;
  const bool lampCmd =
      !strncmp(command, "LAMP:", 5) ||
      !strncmp(command, "LAMP:NODE:", 10);
  if (!lampCmd) return false;
  if (reply && replyLen) reply[0] = '\0';

  const bool alwaysOk =
      !strcmp(command, "LAMP:STATUS") ||
      !strcmp(command, "LAMP:NODE:STATUS") ||
      !strcmp(command, "LAMP:OFF") ||
      !strcmp(command, "LAMP:STOP") ||
      !strcmp(command, "LAMP:NODE:STOP");
  if (emergencyLocked && !alwaysOk) {
    if (reply && replyLen) strncpy(reply, "REJECTED:LAMP:EMERGENCY_ACTIVE", replyLen - 1);
    return true;
  }
  if (!sSt.online && !alwaysOk) {
    if (reply && replyLen) strncpy(reply, "REJECTED:LAMP:OFFLINE", replyLen - 1);
    return true;
  }

  const uint32_t seq = sSeq++;
  sSt.pending = true;
  sSt.pendingSeq = seq;
  strncpy(sSt.lastLife, "PENDING", sizeof(sSt.lastLife) - 1);
  route(seq, command);
  if (reply && replyLen) {
    snprintf(reply, replyLen, "LAMP:PENDING:%lu", (unsigned long)seq);
  }
  return true;
}

void lampNodeLinkOnEmergency(bool active) {
  route(0, active ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR");
  if (active) {
    strncpy(sSt.state, "EMERGENCY", sizeof(sSt.state) - 1);
    sSt.fxActive = false;
    sSt.pending = false;
  }
  publishState();
}

const LampNodeStatus &lampNodeLinkStatus() { return sSt; }

void lampNodeLinkAppendJson(String &json) {
  const uint32_t age = (sSt.lastRxMs && sSt.online) ? (millis() - sSt.lastRxMs) : 0;
  json += "{\n";
  json += "    \"name\": \"Lamp Node\",\n";
  json += "    \"type\": \"LAMP\",\n";
  json += "    \"seen\": ";
  json += sSt.seen ? "true" : "false";
  json += ",\n    \"online\": ";
  json += sSt.online ? "true" : "false";
  json += ",\n    \"pending\": ";
  json += sSt.pending ? "true" : "false";
  json += ",\n    \"state\": \"";
  json += sSt.state;
  json += "\",\n    \"fx\": \"";
  json += sSt.fx;
  json += "\",\n    \"fxActive\": ";
  json += sSt.fxActive ? "true" : "false";
  json += ",\n    \"brightness\": ";
  json += String((unsigned)sSt.brightness);
  json += ",\n    \"mac\": \"";
  json += sSt.mac;
  json += "\",\n    \"firmware\": \"";
  json += sSt.firmware;
  json += "\",\n    \"lastLife\": \"";
  json += sSt.lastLife;
  json += "\",\n    \"lastError\": \"";
  json += sSt.lastError;
  json += "\",\n    \"lastContactMs\": ";
  json += String((unsigned long)age);
  json += "\n  }";
}
