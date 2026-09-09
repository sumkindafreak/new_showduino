#include "LampProtocol.h"
#include "LampEngine.h"
#include "LampNodeState.h"
#include "EspNowLampTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_log.h"

static uint32_t sLastAnnounce = 0;
static uint32_t sActiveSeq = 0;

static bool isRoutineLampReport(const char *line) {
  if (!line) return true;
  if (!strncmp(line, "ANNOUNCE:", 9)) return true;
  if (!strncmp(line, "STATUS:", 7)) return true;
  if (!strncmp(line, "LAMP:OWNED:", 11)) return true;
  if (!strncmp(line, "LAMP:CAPS:", 10)) return true;
  return false;
}

static void report(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  if (isRoutineLampReport(line)) {
    SD_LOGT("LAMP", "%s", line);
  } else if (strstr(line, "EMERGENCY") || strstr(line, "FAILED") || strstr(line, "FAULT")) {
    SD_LOGW("LAMP", "%s", line);
  } else {
    SD_LOGI("LAMP", "%s", line);
  }
  lampNodeStateSetLastResult(line);
  lampEspNowSend(line, seq);
}

static void formatAnnounce(char *out, size_t n) {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  snprintf(out, n, "ANNOUNCE:%s:%s:%s", mac, SHOWDUINO_LAMP_NODE_FW,
           lampNodeStateName());
}

void lampProtocolFormatStatus(char *out, size_t n) {
  snprintf(out, n, "STATUS:%s:B%u:%s:%s",
           lampNodeStateName(),
           (unsigned)lampEngineBrightness(),
           lampEngineActive() ? lampEngineFxToken() : "-",
           lampNodeStateFault());
}

void lampProtocolBegin() {
  sLastAnnounce = 0;
  sActiveSeq = 0;
}

static void goIdle() {
  lampEngineOff();
  if (lampNodeState() == SHOWDUINO_LAMP_ST_FAULT ||
      lampNodeState() == SHOWDUINO_LAMP_ST_EMERGENCY) {
    return;
  }
  if (lampNodeStateShowControlled() &&
      lampNodeStateAuthorityFresh(SHOWDUINO_LAMP_COMMS_TIMEOUT_MS)) {
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED);
  } else {
    lampNodeStateSetShowControlled(false);
    lampNodeStateSet(lampEspNowHaveComms() ? SHOWDUINO_LAMP_ST_STANDALONE
                                           : SHOWDUINO_LAMP_ST_SEARCHING);
  }
}

static void startFx(const ShowduinoLampCommand &parsed, uint32_t sequence, bool fromShow) {
  if (parsed.brightness <= SHOWDUINO_LAMP_BRI_MAX) {
    lampEngineSetBrightness(parsed.brightness);
  }
  if (parsed.cmd == SHOWDUINO_LAMP_CMD_SOLID) {
    lampEngineSetSolid(parsed.r, parsed.g, parsed.b);
  } else {
    lampEngineSetFx(parsed.fx, parsed.speed, parsed.intensity);
  }
  if (fromShow) {
    lampNodeStateSetShowControlled(true);
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED);
  } else if (lampNodeState() != SHOWDUINO_LAMP_ST_SHOW_CONTROLLED) {
    lampNodeStateSet(SHOWDUINO_LAMP_ST_STANDALONE);
  }
  sActiveSeq = sequence;
  char acc[48];
  snprintf(acc, sizeof(acc), "LAMP:ACCEPTED:%lu", (unsigned long)sequence);
  report(acc, sequence);
  char fx[64];
  snprintf(fx, sizeof(fx), "LAMP:FX:%s", lampEngineFxToken());
  report(fx, sequence);
  char started[48];
  snprintf(started, sizeof(started), "LAMP:STARTED:%lu", (unsigned long)sequence);
  report(started, sequence);
}

void lampProtocolApply(const char *command, uint32_t sequence, bool fromShow) {
  if (!command || !command[0]) return;
  lampNodeStateSetLastCommand(command);
  if (fromShow) lampNodeStateNoteComms();

  ShowduinoLampCommand parsed;
  const ShowduinoLampCmd cmd = showduino_lamp_parse_command(command, &parsed);
  if (cmd == SHOWDUINO_LAMP_CMD_NONE) {
    char line[64];
    snprintf(line, sizeof(line), "LAMP:FAILED:%lu:%s",
             (unsigned long)sequence,
             showduino_lamp_fail_name(SHOWDUINO_LAMP_FAIL_BAD_COMMAND));
    report(line, sequence);
    return;
  }

  const ShowduinoCmdOrigin origin =
      fromShow ? SHOWDUINO_CMD_ORIGIN_SHOW : SHOWDUINO_CMD_ORIGIN_LOCAL;
  const ShowduinoLampFail gate =
      showduino_lamp_can_accept_ex(lampNodeState(), cmd, origin);
  if (gate != SHOWDUINO_LAMP_FAIL_NONE) {
    char line[64];
    snprintf(line, sizeof(line), "LAMP:FAILED:%lu:%s",
             (unsigned long)sequence, showduino_lamp_fail_name(gate));
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_OWN_GRANT) {
    lampNodeStateSetShowControlled(true);
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED);
    char line[48];
    snprintf(line, sizeof(line), "LAMP:OWNED:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_EMERGENCY_STOP) {
    lampEngineOnEmergency(true);
    lampNodeStateSetShowControlled(false);
    lampNodeStateSet(SHOWDUINO_LAMP_ST_EMERGENCY);
    char line[48];
    snprintf(line, sizeof(line), "LAMP:EMERGENCY:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_EMERGENCY_CLEAR) {
    lampEngineOnEmergency(false);
    lampNodeStateSetShowControlled(false);
    if (lampNodeState() != SHOWDUINO_LAMP_ST_FAULT) {
      lampNodeStateSet(lampEspNowHaveComms() ? SHOWDUINO_LAMP_ST_STANDALONE
                                             : SHOWDUINO_LAMP_ST_SEARCHING);
    }
    char line[48];
    snprintf(line, sizeof(line), "LAMP:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_OFF || cmd == SHOWDUINO_LAMP_CMD_STOP) {
    goIdle();
    char line[48];
    snprintf(line, sizeof(line), "LAMP:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_BRIGHTNESS) {
    lampEngineSetBrightness(parsed.brightness);
    char line[32];
    snprintf(line, sizeof(line), "LAMP:BRIGHTNESS:%u",
             (unsigned)lampEngineBrightness());
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_STATUS) {
    char line[96];
    lampProtocolFormatStatus(line, sizeof(line));
    report(line, sequence);
    char caps[96];
    snprintf(caps, sizeof(caps), "LAMP:CAPS:%s", SHOWDUINO_LAMP_CAPS);
    report(caps, sequence);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_LIST) {
    uint16_t start = 0, count = 0;
    showduino_lamp_list_slice(0, &start, &count);
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    size_t n = (size_t)snprintf(line, sizeof(line), "LAMP:LIST:0:%u:",
                                (unsigned)SHOWDUINO_LAMP_FX_COUNT);
    for (uint16_t i = 0; i < count && n + 2 < sizeof(line); i++) {
      const ShowduinoLampFxInfo *info =
          showduino_lamp_fx_info((ShowduinoLampFx)(start + i));
      n += (size_t)snprintf(line + n, sizeof(line) - n, "%s%s",
                            i ? "," : "", info ? info->token : "?");
    }
    report(line, sequence);
    uint16_t page = 1;
    for (;;) {
      showduino_lamp_list_slice(page, &start, &count);
      if (count == 0) break;
      n = (size_t)snprintf(line, sizeof(line), "LAMP:LIST:%u:%u:",
                           (unsigned)page, (unsigned)SHOWDUINO_LAMP_FX_COUNT);
      for (uint16_t i = 0; i < count && n + 2 < sizeof(line); i++) {
        const ShowduinoLampFxInfo *info =
            showduino_lamp_fx_info((ShowduinoLampFx)(start + i));
        n += (size_t)snprintf(line + n, sizeof(line) - n, "%s%s",
                              i ? "," : "", info ? info->token : "?");
      }
      report(line, sequence);
      page++;
      if (page > 8) break;
    }
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_TEST) {
    parsed.cmd = SHOWDUINO_LAMP_CMD_FX;
    parsed.fx = SHOWDUINO_LAMP_FX_CARBIDE_FLAME;
    parsed.speed = 0;
    parsed.intensity = 255;
    startFx(parsed, sequence, fromShow);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_SOLID || cmd == SHOWDUINO_LAMP_CMD_FX) {
    startFx(parsed, sequence, fromShow);
  }
}

void lampProtocolLocalTest() {
  if (lampNodeState() == SHOWDUINO_LAMP_ST_EMERGENCY) {
    Serial.println("[LAMP] Local test blocked — emergency");
    return;
  }
  if (lampNodeStateShowControlled()) {
    Serial.println("[LAMP] Local test blocked — P4 show control");
    return;
  }
  if (lampEngineActive()) {
    Serial.println("[LAMP] Local test: stop");
    lampProtocolApply("LAMP:OFF", 0, false);
    return;
  }
  Serial.println("[LAMP] Local test: CARBIDE_FLAME");
  lampProtocolApply("LAMP:TEST", 0, false);
}

void lampProtocolLocalStop() {
  if (lampNodeState() == SHOWDUINO_LAMP_ST_EMERGENCY) return;
  lampProtocolApply("LAMP:OFF", 0, false);
}

void lampProtocolAnnounce() {
  char line[96];
  formatAnnounce(line, sizeof(line));
  lampEspNowSend(line, 0);
}

void lampProtocolService() {
  if (lampNodeStateShowControlled() &&
      !lampNodeStateAuthorityFresh(SHOWDUINO_LAMP_COMMS_TIMEOUT_MS) &&
      lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY) {
    Serial.println("[LAMP] Comms timeout — safe idle");
    lampNodeStateSetShowControlled(false);
    goIdle();
    char line[64];
    snprintf(line, sizeof(line), "LAMP:FAILED:%lu:%s",
             (unsigned long)sActiveSeq,
             showduino_lamp_fail_name(SHOWDUINO_LAMP_FAIL_COMMS_TIMEOUT));
    report(line, sActiveSeq);
  }

  if (lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY &&
      lampNodeState() != SHOWDUINO_LAMP_ST_FAULT &&
      lampNodeState() != SHOWDUINO_LAMP_ST_SHOW_CONTROLLED &&
      lampNodeState() != SHOWDUINO_LAMP_ST_STANDALONE) {
    const ShowduinoLampNodeState want =
        lampEspNowHaveComms() ? SHOWDUINO_LAMP_ST_STANDALONE
                              : SHOWDUINO_LAMP_ST_SEARCHING;
    if (lampNodeState() != want) lampNodeStateSet(want);
  }

  if ((millis() - sLastAnnounce) >= SHOWDUINO_LAMP_ANNOUNCE_MS) {
    sLastAnnounce = millis();
    lampProtocolAnnounce();
  }
}
