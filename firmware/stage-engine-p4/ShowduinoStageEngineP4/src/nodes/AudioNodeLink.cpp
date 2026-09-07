#include "AudioNodeLink.h"
#include "../storage/StageLog.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_sound_input.h"

extern bool emergencyLocked;
bool sendToDirector(const String &message);
void stageCommsSendLine(const char *line);

static AudioNodeStatus sSt;
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static uint32_t sInvMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";
static char sLastInv[96] = "";
static char sLastCaps[96] = "";
static char sLastMeta[96] = "";
static char sLastSound[96] = "";

static const char *shortState(const char *state, bool online) {
  if (!online) return "OFF";
  if (!state) return "IDLE";
  if (!strcmp(state, "PLAYING") || !strcmp(state, "LOADING")) return "PLAY";
  if (!strcmp(state, "LOOPING")) return "LOOP";
  if (!strcmp(state, "PAUSED")) return "PAUS";
  if (!strcmp(state, "FAULT")) return "FLT";
  if (!strcmp(state, "EMERGENCY")) return "ESTP";
  if (!strcmp(state, "NO_STORAGE")) return "NSD";
  if (!strcmp(state, "STOPPING")) return "STOP";
  if (!strcmp(state, "OFFLINE")) return "OFF";
  return "IDLE";
}

static const char *shortStorage(const char *storage, bool online) {
  if (!online) return "OFF";
  if (!strcmp(sSt.state, "NO_STORAGE")) return "OFF";
  if (!storage || !storage[0]) return "ON";
  if (!strcmp(storage, "OFFLINE") || !strcmp(storage, "NO_STORAGE")) return "OFF";
  if (!strcmp(storage, "DEGRADED") || !strcmp(storage, "FAULT") ||
      !strcmp(storage, "READ_ONLY")) {
    return "FLT";
  }
  return "ON";
}

static const char *shortFault(const char *err) {
  if (!err || !err[0] || !strcmp(err, "NONE") || !strcmp(err, "-")) return "-";
  if (!strcmp(err, "FILE_NOT_FOUND")) return "FILE";
  if (!strcmp(err, "BAD_PATH")) return "PATH";
  if (!strcmp(err, "CODEC")) return "CODEC";
  if (!strcmp(err, "NO_STORAGE") || !strcmp(err, "STORAGE")) return "STOR";
  if (!strcmp(err, "COMMS_TIMEOUT")) return "TO";
  if (!strcmp(err, "UNSUPPORTED")) return "UNSUP";
  if (!strcmp(err, "CONFIG_FAULT")) return "CFG";
  if (!strcmp(err, "BAD_COMMAND")) return "CMD";
  return "FLT";
}

static void publishExtra(bool force) {
  char line[96];
  char asset[SHOWDUINO_AUDIO_DETAIL_ASSET_MAX + 1];
  const char *src = (sSt.online && sSt.asset[0] && strcmp(sSt.asset, "-") != 0) ? sSt.asset : "-";
  strncpy(asset, src, sizeof(asset) - 1);
  asset[sizeof(asset) - 1] = '\0';
  snprintf(line, sizeof(line), "%s%s:%u:%s:%s:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_AUDIO_DETAIL_PREFIX,
           shortState(sSt.state, sSt.online),
           (unsigned)sSt.volume,
           shortStorage(sSt.storage, sSt.online),
           (!sSt.online) ? "-" : (!strcmp(sSt.lastError, "CODEC") ? "FLT" : "OK"),
           shortFault(sSt.lastError),
           asset);
  if (force || strcmp(sLastDetail, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastDetail, line, sizeof(sLastDetail) - 1);
    }
  }

  snprintf(line, sizeof(line), "%sWAV,PLAY,LOOP,STOP,VOL,PAUSE,FADE,DUCK,INV,MIC",
           SHOWDUINO_WIRE_STATE_NODE_AUDIO_CAPS_PREFIX);
  if (sSt.capabilities[0] && !strstr(sSt.capabilities, "PAUSE")) {
    snprintf(line, sizeof(line), "%sWAV,PLAY,LOOP,STOP,VOL,MIC",
             SHOWDUINO_WIRE_STATE_NODE_AUDIO_CAPS_PREFIX);
  }
  if (force || strcmp(sLastCaps, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastCaps, line, sizeof(sLastCaps) - 1);
    }
  }

  if (sSt.firmware[0] || sSt.mac[0]) {
    snprintf(line, sizeof(line), "%s%s,%s",
             SHOWDUINO_WIRE_STATE_NODE_AUDIO_META_PREFIX,
             sSt.firmware[0] ? sSt.firmware : "-",
             sSt.mac[0] ? sSt.mac : "-");
    if (force || strcmp(sLastMeta, line) != 0) {
      if (sendToDirector(String(line))) {
        strncpy(sLastMeta, line, sizeof(sLastMeta) - 1);
      }
    }
  }

  {
    char names[80] = "";
    size_t n = 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_PER_PAGE && count < SHOWDUINO_AUDIO_INV_WIRE_MAX; i++) {
      if (!sSt.inventory[i][0]) continue;
      char one[21];
      strncpy(one, sSt.inventory[i], sizeof(one) - 1);
      one[sizeof(one) - 1] = '\0';
      n += (size_t)snprintf(names + n, sizeof(names) - n, "%s%s", count ? "," : "", one);
      count++;
    }
    snprintf(line, sizeof(line), "%s%u:%u:%s",
             SHOWDUINO_WIRE_STATE_NODE_AUDIO_INV_PREFIX,
             (unsigned)sSt.inventoryPage, (unsigned)sSt.inventoryTotal, names);
    if ((force || strcmp(sLastInv, line) != 0) && (count || sSt.inventoryTotal || force)) {
      if (sendToDirector(String(line))) {
        strncpy(sLastInv, line, sizeof(sLastInv) - 1);
      }
    }
  }

  snprintf(line, sizeof(line), "%s%s,L=%u,P=%u,F=%u,T=%u,A=%u,C=%u,E=%s,K=%u",
           SHOWDUINO_WIRE_STATE_NODE_AUDIO_SOUND_PREFIX,
           sSt.soundReadyTok[0] ? sSt.soundReadyTok : "OFF",
           (unsigned)sSt.soundLevel, (unsigned)sSt.soundPeak,
           (unsigned)sSt.soundFloor, (unsigned)sSt.soundThreshold,
           sSt.soundArmed ? 1 : 0, (unsigned)sSt.soundCooldown,
           sSt.soundLastType[0] ? sSt.soundLastType : "NONE",
           sSt.soundCalibrated ? 1 : 0);
  if (force || strcmp(sLastSound, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastSound, line, sizeof(sLastSound) - 1);
    }
  }
}

static ShowduinoAudioNodeState stateFromName(const char *name) {
  if (!name) return SHOWDUINO_AUDIO_ST_UNKNOWN;
  if (!strcmp(name, "OFFLINE")) return SHOWDUINO_AUDIO_ST_OFFLINE;
  if (!strcmp(name, "BOOTING")) return SHOWDUINO_AUDIO_ST_BOOTING;
  if (!strcmp(name, "IDLE")) return SHOWDUINO_AUDIO_ST_IDLE;
  if (!strcmp(name, "LOADING")) return SHOWDUINO_AUDIO_ST_LOADING;
  if (!strcmp(name, "PLAYING")) return SHOWDUINO_AUDIO_ST_PLAYING;
  if (!strcmp(name, "LOOPING")) return SHOWDUINO_AUDIO_ST_LOOPING;
  if (!strcmp(name, "PAUSED")) return SHOWDUINO_AUDIO_ST_PAUSED;
  if (!strcmp(name, "STOPPING")) return SHOWDUINO_AUDIO_ST_STOPPING;
  if (!strcmp(name, "EMERGENCY")) return SHOWDUINO_AUDIO_ST_EMERGENCY;
  if (!strcmp(name, "FAULT")) return SHOWDUINO_AUDIO_ST_FAULT;
  if (!strcmp(name, "NO_STORAGE")) return SHOWDUINO_AUDIO_ST_NO_STORAGE;
  return SHOWDUINO_AUDIO_ST_UNKNOWN;
}

static void publishState(bool force = false) {
  char line[48];
  const char *tok = sSt.online ? showduino_audio_wire_token(stateFromName(sSt.state))
                               : "OFFLINE";
  if (!force && !strcmp(sLastWire, tok)) return;
  snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_AUDIO_PREFIX, tok);
  if (!sendToDirector(String(line))) return;
  strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
  sLastWire[sizeof(sLastWire) - 1] = '\0';
  sPublishMs = millis();
  publishExtra(force);
}

void audioNodeLinkPublishToDirector() {
  publishState(true);
}

static void markRx() {
  sSt.seen = true;
  sSt.online = true;
  sSt.lastRxMs = millis();
}

static void route(uint32_t seq, const char *cmd) {
  char line[180];
  snprintf(line, sizeof(line), "ROUTE:AUDIO:%lu:%s", (unsigned long)seq, cmd ? cmd : "");
  stageCommsSendLine(line);
}

void audioNodeLinkBegin() {
  sSt = AudioNodeStatus();
  sLastWire[0] = '\0';
  sLastDetail[0] = '\0';
  sLastInv[0] = '\0';
  sLastCaps[0] = '\0';
  sLastMeta[0] = '\0';
  sLastSound[0] = '\0';
}

void audioNodeLinkLoop() {
  if (sSt.online && sSt.lastRxMs &&
      (millis() - sSt.lastRxMs) > 8000UL) {
    sSt.online = false;
    strncpy(sSt.state, "OFFLINE", sizeof(sSt.state) - 1);
    sSt.pending = false;
    sSt.logicalInput[0] = '\0';
    sSt.logicalSubtype[0] = '\0';
    sSt.soundArmed = false;
    publishState();
  }
  if (sSt.online && (millis() - sKeepaliveMs) >= 2000UL) {
    sKeepaliveMs = millis();
    route(sSeq++, "AUDIO:NODE:STATUS");
  }
  if (sSt.online && (millis() - sInvMs) >= 15000UL) {
    sInvMs = millis();
    route(sSeq++, "AUDIO:NODE:INVENTORY");
  }
  if ((millis() - sPublishMs) >= 3000UL) {
    publishState(true);
  }
}

bool audioNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:AUDIO:", 11) != 0) return false;
  const char *p = line + 11;
  markRx();
  Serial.printf("[AUDIO] node RX %s\n", p);

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
    if (sInvMs == 0) {
      sInvMs = millis();
      route(sSeq++, "AUDIO:NODE:INVENTORY");
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "STATUS:", 7)) {
    char buf[96];
    strncpy(buf, p + 7, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    char *st = strtok_r(buf, ":", &save);
    char *vol = strtok_r(nullptr, ":", &save);
    char *asset = strtok_r(nullptr, ":", &save);
    char *fault = strtok_r(nullptr, ":", &save);
    if (st) strncpy(sSt.state, st, sizeof(sSt.state) - 1);
    if (vol && vol[0] == 'V') sSt.volume = (uint8_t)atoi(vol + 1);
    if (asset && strcmp(asset, "-") != 0) strncpy(sSt.asset, asset, sizeof(sSt.asset) - 1);
    else if (asset) sSt.asset[0] = '\0';
    if (fault && strcmp(fault, "-") != 0 && strcmp(fault, "NONE") != 0) {
      strncpy(sSt.lastError, fault, sizeof(sSt.lastError) - 1);
    } else {
      sSt.lastError[0] = '\0';
    }
    publishState();
    return true;
  }

  if (!strncmp(p, "AUDIO:CAPS:", 11)) {
    strncpy(sSt.capabilities, p + 11, sizeof(sSt.capabilities) - 1);
    return true;
  }
  if (!strncmp(p, "AUDIO:META:", 11)) {
    char buf[64];
    strncpy(buf, p + 11, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    char *codec = strtok_r(buf, ":", &save);
    char *storage = strtok_r(nullptr, ":", &save);
    char *output = strtok_r(nullptr, ":", &save);
    if (codec) strncpy(sSt.codec, codec, sizeof(sSt.codec) - 1);
    if (storage) strncpy(sSt.storage, storage, sizeof(sSt.storage) - 1);
    if (output) strncpy(sSt.output, output, sizeof(sSt.output) - 1);
    return true;
  }
  if (!strncmp(p, "AUDIO:INVENTORY:", 16)) {
    char buf[SHOWDUINO_NODE_COMMAND_MAX];
    strncpy(buf, p + 16, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *save = nullptr;
    char *page = strtok_r(buf, ":", &save);
    char *total = strtok_r(nullptr, ":", &save);
    char *list = strtok_r(nullptr, ":", &save);
    if (page) sSt.inventoryPage = (uint16_t)atoi(page);
    if (total) sSt.inventoryTotal = (uint16_t)atoi(total);
    for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_PER_PAGE; i++) sSt.inventory[i][0] = '\0';
    if (list && list[0]) {
      uint8_t i = 0;
      char *item = list;
      while (item && *item && i < SHOWDUINO_AUDIO_INV_PER_PAGE) {
        char *comma = strchr(item, ',');
        if (comma) *comma = '\0';
        strncpy(sSt.inventory[i], item, sizeof(sSt.inventory[i]) - 1);
        sSt.inventory[i][sizeof(sSt.inventory[i]) - 1] = '\0';
        i++;
        item = comma ? comma + 1 : nullptr;
      }
    }
    publishExtra(false);
    return true;
  }

  if (!strncmp(p, "AUDIO:ACCEPTED:", 15)) {
    sSt.lastSeq = (uint32_t)strtoul(p + 15, nullptr, 10);
    strncpy(sSt.lastLife, "ACCEPTED", sizeof(sSt.lastLife) - 1);
    sSt.pending = true;
    return true;
  }
  if (!strncmp(p, "AUDIO:STARTED:", 14)) {
    char *end = nullptr;
    sSt.lastSeq = (uint32_t)strtoul(p + 14, &end, 10);
    if (end && *end == ':') strncpy(sSt.asset, end + 1, sizeof(sSt.asset) - 1);
    strncpy(sSt.state, "PLAYING", sizeof(sSt.state) - 1);
    strncpy(sSt.lastLife, "STARTED", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    publishState();
    return true;
  }
  if (!strncmp(p, "AUDIO:COMPLETED:", 16)) {
    char *end = nullptr;
    sSt.lastSeq = (uint32_t)strtoul(p + 16, &end, 10);
    strncpy(sSt.state, "IDLE", sizeof(sSt.state) - 1);
    strncpy(sSt.lastLife, "COMPLETED", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    publishState();
    return true;
  }
  if (!strncmp(p, "AUDIO:FAILED:", 13)) {
    char *end = nullptr;
    sSt.lastSeq = (uint32_t)strtoul(p + 13, &end, 10);
    if (end && *end == ':') strncpy(sSt.lastError, end + 1, sizeof(sSt.lastError) - 1);
    strncpy(sSt.lastLife, "FAILED", sizeof(sSt.lastLife) - 1);
    if (!strcmp(sSt.lastError, "NO_STORAGE")) {
      strncpy(sSt.state, "NO_STORAGE", sizeof(sSt.state) - 1);
    } else if (strcmp(sSt.state, "EMERGENCY") != 0 && strcmp(sSt.state, "FAULT") != 0) {
      strncpy(sSt.state, "IDLE", sizeof(sSt.state) - 1);
    }
    sSt.pending = false;
    publishState();
    return true;
  }
  if (!strncmp(p, "AUDIO:VOLUME:", 13)) {
    sSt.volume = (uint8_t)atoi(p + 13);
    sSt.pending = false;
    strncpy(sSt.lastLife, "VOLUME", sizeof(sSt.lastLife) - 1);
    return true;
  }
  if (!strncmp(p, "AUDIO:PAUSED:", 13)) {
    strncpy(sSt.state, "PAUSED", sizeof(sSt.state) - 1);
    strncpy(sSt.lastLife, "PAUSED", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    publishState();
    return true;
  }
  if (!strncmp(p, "AUDIO:IDLE:", 11)) {
    strncpy(sSt.state, "IDLE", sizeof(sSt.state) - 1);
    strncpy(sSt.lastLife, "IDLE", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    publishState();
    return true;
  }
  if (!strncmp(p, "AUDIO:EMERGENCY:", 16)) {
    strncpy(sSt.state, "EMERGENCY", sizeof(sSt.state) - 1);
    strncpy(sSt.lastLife, "EMERGENCY", sizeof(sSt.lastLife) - 1);
    sSt.pending = false;
    sSt.logicalInput[0] = '\0';
    sSt.logicalSubtype[0] = '\0';
    sSt.soundArmed = false;
    publishState();
    return true;
  }

  if (!strncmp(p, "SOUND:STATUS:", 13)) {
    const char *q = p + 13;
    const char *comma = strchr(q, ',');
    size_t n = comma ? (size_t)(comma - q) : strlen(q);
    if (n >= sizeof(sSt.soundReadyTok)) n = sizeof(sSt.soundReadyTok) - 1;
    memcpy(sSt.soundReadyTok, q, n);
    sSt.soundReadyTok[n] = '\0';
    sSt.soundReady = strcmp(sSt.soundReadyTok, "RDY") == 0 ||
                     strcmp(sSt.soundReadyTok, "CAL") == 0;
    sSt.soundEnabled = strcmp(sSt.soundReadyTok, "OFF") != 0;
    uint32_t v = 0;
    if (showduino_parse_kv_u32(q, "L", &v)) sSt.soundLevel = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "P", &v)) sSt.soundPeak = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "F", &v)) sSt.soundFloor = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "T", &v)) sSt.soundThreshold = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "A", &v)) sSt.soundArmed = v != 0;
    if (showduino_parse_kv_u32(q, "C", &v)) sSt.soundCooldown = (uint16_t)v;
    if (showduino_parse_kv_u32(q, "K", &v)) sSt.soundCalibrated = v != 0;
    const char *e = strstr(q, "E=");
    if (e) {
      e += 2;
      comma = strchr(e, ',');
      n = comma ? (size_t)(comma - e) : strlen(e);
      if (n >= sizeof(sSt.soundLastType)) n = sizeof(sSt.soundLastType) - 1;
      memcpy(sSt.soundLastType, e, n);
      sSt.soundLastType[n] = '\0';
    }
    publishExtra(false);
    return true;
  }

  if (!strncmp(p, "SOUND:TRIGGER:", 14)) {
    if (emergencyLocked) {
      sSt.logicalInput[0] = '\0';
      sSt.logicalSubtype[0] = '\0';
      return true;
    }
    const char *q = p + 14;
    char type[12] = "";
    const char *comma = strchr(q, ',');
    size_t n = comma ? (size_t)(comma - q) : strlen(q);
    if (n >= sizeof(type)) n = sizeof(type) - 1;
    memcpy(type, q, n);
    type[n] = '\0';
    uint32_t v = 0;
    if (showduino_parse_kv_u32(q, "ID", &v)) sSt.soundLastId = v;
    if (showduino_parse_kv_u32(q, "L", &v)) sSt.soundLevel = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "P", &v)) sSt.soundPeak = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "F", &v)) sSt.soundFloor = (uint8_t)v;
    if (showduino_parse_kv_u32(q, "TH", &v)) sSt.soundThreshold = (uint8_t)v;
    strncpy(sSt.soundLastType, type, sizeof(sSt.soundLastType) - 1);
    sSt.soundLastMs = millis();
    strncpy(sSt.logicalInput, SHOWDUINO_LOGICAL_INPUT_SOUND, sizeof(sSt.logicalInput) - 1);
    strncpy(sSt.logicalSubtype, type, sizeof(sSt.logicalSubtype) - 1);
    char log[96];
    snprintf(log, sizeof(log), "[SOUND] Audio Node %s level=%u floor=%u",
             type[0] ? type : "EVENT",
             (unsigned)sSt.soundLevel, (unsigned)sSt.soundFloor);
    stageLogWrite(StageLogChannel::System, "INFO", log);
    Serial.println(log);
    publishExtra(true);
    return true;
  }

  if (!strncmp(p, "SOUND:CALIBRATE:", 16)) {
    strncpy(sSt.soundReadyTok, "CAL", sizeof(sSt.soundReadyTok) - 1);
    if (strstr(p, "DONE")) {
      strncpy(sSt.soundReadyTok, "RDY", sizeof(sSt.soundReadyTok) - 1);
      sSt.soundCalibrated = true;
      uint32_t v = 0;
      if (showduino_parse_kv_u32(p, "F", &v)) sSt.soundFloor = (uint8_t)v;
    }
    publishExtra(true);
    return true;
  }
  return true;
}

bool audioNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command || strncmp(command, "AUDIO:NODE:", 11) != 0) return false;
  if (reply && replyLen) reply[0] = '\0';

  if (!strncmp(command, "AUDIO:NODE:SOUND:", 17)) {
    const char *sub = command + 17;
    if (!strncmp(sub, "LOCAL_TEST_TRIGGER", 18)) {
      if (reply && replyLen) strncpy(reply, "ERR:AUDIO:NODE:SOUND:SHOW_ONLY", replyLen - 1);
      return true;
    }
    const bool diagOk = !strcmp(sub, "STATUS") || !strcmp(sub, "LEVEL") ||
                        !strcmp(sub, "CONFIG") || !strcmp(sub, "CALIBRATE") ||
                        !strcmp(sub, "DISABLE") || !strncmp(sub, "THRESHOLD:", 10);
    if (emergencyLocked && !diagOk) {
      if (reply && replyLen) strncpy(reply, "REJECTED:AUDIO:NODE:EMERGENCY_ACTIVE", replyLen - 1);
      return true;
    }
    if (!sSt.online && strcmp(sub, "STATUS") != 0) {
      if (reply && replyLen) strncpy(reply, "REJECTED:AUDIO:NODE:OFFLINE", replyLen - 1);
      return true;
    }
    const uint32_t seq = sSeq++;
    sSt.pending = true;
    sSt.pendingSeq = seq;
    strncpy(sSt.lastLife, "PENDING", sizeof(sSt.lastLife) - 1);
    route(seq, command);
    if (reply && replyLen) {
      snprintf(reply, replyLen, "AUDIO:NODE:PENDING:%lu", (unsigned long)seq);
    }
    return true;
  }

  char arg[80];
  int vol = -1;
  int fade = -1;
  int pri = -1;
  const ShowduinoAudioCmd cmd =
      showduino_audio_parse_command_ex(command, arg, sizeof(arg), &vol, &fade, &pri);
  if (cmd == SHOWDUINO_AUDIO_CMD_NONE || cmd == SHOWDUINO_AUDIO_CMD_LOCAL_REJECT) {
    if (reply && replyLen) strncpy(reply, "ERR:AUDIO:NODE:BAD_COMMAND", replyLen - 1);
    return true;
  }

  const bool alwaysOk = (cmd == SHOWDUINO_AUDIO_CMD_STOP ||
                         cmd == SHOWDUINO_AUDIO_CMD_STATUS ||
                         cmd == SHOWDUINO_AUDIO_CMD_INVENTORY);
  if (emergencyLocked && !alwaysOk) {
    if (reply && replyLen) strncpy(reply, "REJECTED:AUDIO:NODE:EMERGENCY_ACTIVE", replyLen - 1);
    return true;
  }
  if (!sSt.online && !alwaysOk) {
    if (reply && replyLen) strncpy(reply, "REJECTED:AUDIO:NODE:OFFLINE", replyLen - 1);
    return true;
  }

  if ((cmd == SHOWDUINO_AUDIO_CMD_PLAY || cmd == SHOWDUINO_AUDIO_CMD_LOOP ||
       cmd == SHOWDUINO_AUDIO_CMD_TEST) && arg[0]) {
    char absPath[SHOWDUINO_AUDIO_PATH_MAX + 1];
    if (showduino_audio_resolve_path(arg, absPath, sizeof(absPath)) != SHOWDUINO_AUDIO_PATH_OK) {
      if (reply && replyLen) strncpy(reply, "ERR:AUDIO:NODE:BAD_PATH", replyLen - 1);
      return true;
    }
  }

  const uint32_t seq = sSeq++;
  sSt.pending = true;
  sSt.pendingSeq = seq;
  strncpy(sSt.lastLife, "PENDING", sizeof(sSt.lastLife) - 1);
  route(seq, command);
  if (reply && replyLen) {
    snprintf(reply, replyLen, "AUDIO:NODE:PENDING:%lu", (unsigned long)seq);
  }
  return true;
}

void audioNodeLinkOnEmergency(bool active) {
  route(0, active ? "EMERGENCY:STOP" : "EMERGENCY:CLEAR");
  if (active) {
    strncpy(sSt.state, "EMERGENCY", sizeof(sSt.state) - 1);
    sSt.pending = false;
    sSt.logicalInput[0] = '\0';
    sSt.logicalSubtype[0] = '\0';
    sSt.soundArmed = false;
  }
  publishState();
}

const AudioNodeStatus &audioNodeLinkStatus() { return sSt; }

void audioNodeLinkAppendJson(String &json) {
  const uint32_t age = (sSt.lastRxMs && sSt.online) ? (millis() - sSt.lastRxMs) : 0;
  json += "{\n";
  json += "    \"name\": \"Audio Node\",\n";
  json += "    \"type\": \"AUDIO\",\n";
  json += "    \"seen\": ";
  json += sSt.seen ? "true" : "false";
  json += ",\n    \"online\": ";
  json += sSt.online ? "true" : "false";
  json += ",\n    \"pending\": ";
  json += sSt.pending ? "true" : "false";
  json += ",\n    \"state\": \"";
  json += sSt.state;
  json += "\",\n    \"mac\": \"";
  json += sSt.mac;
  json += "\",\n    \"firmware\": \"";
  json += sSt.firmware;
  json += "\",\n    \"protocol\": \"";
  json += SHOWDUINO_AUDIO_PROTOCOL;
  json += "\",\n    \"codec\": \"";
  json += sSt.codec;
  json += "\",\n    \"storage\": \"";
  json += sSt.storage;
  json += "\",\n    \"output\": \"";
  json += sSt.output;
  json += "\",\n    \"volume\": ";
  json += String((unsigned)sSt.volume);
  json += ",\n    \"asset\": \"";
  json += sSt.asset;
  json += "\",\n    \"lastLife\": \"";
  json += sSt.lastLife;
  json += "\",\n    \"lastError\": \"";
  json += sSt.lastError;
  json += "\",\n    \"lastContactMs\": ";
  json += String((unsigned long)age);
  json += ",\n    \"lastSeq\": ";
  json += String((unsigned long)sSt.lastSeq);
  json += ",\n    \"capabilities\": \"";
  json += sSt.capabilities;
  json += "\",\n    \"inventoryTotal\": ";
  json += String((unsigned)sSt.inventoryTotal);
  json += ",\n    \"inventoryPage\": ";
  json += String((unsigned)sSt.inventoryPage);
  json += ",\n    \"soundInput\": {\n";
  json += "      \"ready\": ";
  json += sSt.soundReady ? "true" : "false";
  json += ",\n      \"enabled\": ";
  json += sSt.soundEnabled ? "true" : "false";
  json += ",\n      \"state\": \"";
  json += sSt.soundReadyTok;
  json += "\",\n      \"level\": ";
  json += String((unsigned)sSt.soundLevel);
  json += ",\n      \"peak\": ";
  json += String((unsigned)sSt.soundPeak);
  json += ",\n      \"noiseFloor\": ";
  json += String((unsigned)sSt.soundFloor);
  json += ",\n      \"threshold\": ";
  json += String((unsigned)sSt.soundThreshold);
  json += ",\n      \"armed\": ";
  json += sSt.soundArmed ? "true" : "false";
  json += ",\n      \"calibrated\": ";
  json += sSt.soundCalibrated ? "true" : "false";
  json += ",\n      \"cooldownMs\": ";
  json += String((unsigned)sSt.soundCooldown);
  json += ",\n      \"lastEvent\": \"";
  json += sSt.soundLastType;
  json += "\",\n      \"logicalInput\": \"";
  json += sSt.logicalInput;
  json += "\",\n      \"logicalSubtype\": \"";
  json += sSt.logicalSubtype;
  json += "\"\n    }";
  json += ",\n    \"inventory\": [";
  bool first = true;
  for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_PER_PAGE; i++) {
    if (!sSt.inventory[i][0]) continue;
    if (!first) json += ", ";
    first = false;
    json += "\"";
    json += sSt.inventory[i];
    json += "\"";
  }
  json += "]\n  }";
}
