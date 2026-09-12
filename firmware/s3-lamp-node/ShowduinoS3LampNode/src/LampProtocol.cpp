#include "LampProtocol.h"
#include "LampEngine.h"
#include "LampNodeState.h"
#include "LampConfig.h"
#include "LampSensors.h"
#include "LampWeb.h"
#include "EspNowLampTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_carbide_lamp.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_log.h"

static uint32_t sLastAnnounce = 0;
static uint32_t sActiveSeq = 0;
static uint32_t sApDueMs = 0;

static bool isRoutine(const char *line) {
  if (!line) return true;
  if (!strncmp(line, "ANNOUNCE:", 9)) return true;
  if (!strncmp(line, "STATUS:", 7)) return true;
  if (!strncmp(line, "LAMP:OWNED:", 11)) return true;
  if (!strncmp(line, "LAMP:CAPS:", 10)) return true;
  if (!strncmp(line, "LAMP:ID:", 8)) return true;
  return false;
}

static void report(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  if (isRoutine(line)) {
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
  if (!sApDueMs) sApDueMs = millis() + 400UL;
}

static void markShow(bool fromShow) {
  if (fromShow) {
    lampNodeStateSetShowControlled(true);
    lampNodeStateSet(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED);
  } else if (lampNodeState() != SHOWDUINO_LAMP_ST_SHOW_CONTROLLED) {
    lampNodeStateSet(SHOWDUINO_LAMP_ST_STANDALONE);
  }
}

static void accepted(uint32_t sequence, bool fromShow) {
  markShow(fromShow);
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

static void goIdle() {
  lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_EXTINGUISH);
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

void lampProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin) {
  if (!command || !command[0]) return;
  lampNodeStateSetLastCommand(command);
  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) lampNodeStateNoteComms();

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

  if (!showduino_lamp_id_matches(parsed.logicalId, lampConfigId())) {
    char line[64];
    snprintf(line, sizeof(line), "LAMP:FAILED:%lu:%s",
             (unsigned long)sequence,
             showduino_lamp_fail_name(SHOWDUINO_LAMP_FAIL_WRONG_ID));
    report(line, sequence);
    return;
  }

  const ShowduinoLampFail gate =
      showduino_lamp_can_accept_ex(lampNodeState(), cmd, origin);
  if (gate != SHOWDUINO_LAMP_FAIL_NONE) {
    char line[64];
    snprintf(line, sizeof(line), "LAMP:FAILED:%lu:%s",
             (unsigned long)sequence, showduino_lamp_fail_name(gate));
    report(line, sequence);
    return;
  }

  const bool fromShow = origin == SHOWDUINO_CMD_ORIGIN_SHOW;

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

  if (cmd == SHOWDUINO_LAMP_CMD_OFF || cmd == SHOWDUINO_LAMP_CMD_STOP ||
      cmd == SHOWDUINO_LAMP_CMD_EXTINGUISH) {
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
    char id[40];
    snprintf(id, sizeof(id), "LAMP:ID:%s", lampConfigId());
    report(id, sequence);
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

  if (cmd == SHOWDUINO_LAMP_CMD_IGNITE || cmd == SHOWDUINO_LAMP_CMD_TEST) {
    if (cmd == SHOWDUINO_LAMP_CMD_TEST && lampEngineFlameLit()) {
      goIdle();
      char line[48];
      snprintf(line, sizeof(line), "LAMP:IDLE:%lu", (unsigned long)sequence);
      report(line, sequence);
      return;
    }
    if (parsed.brightness <= SHOWDUINO_LAMP_BRI_MAX) {
      lampEngineSetBrightness(parsed.brightness);
    }
    lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_IGNITE);
    accepted(sequence, fromShow);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_SOLID) {
    if (parsed.brightness <= SHOWDUINO_LAMP_BRI_MAX) {
      lampEngineSetBrightness(parsed.brightness);
    }
    lampEngineSetSolid(parsed.r, parsed.g, parsed.b);
    accepted(sequence, fromShow);
    return;
  }

  if (cmd == SHOWDUINO_LAMP_CMD_FX) {
    if (parsed.brightness <= SHOWDUINO_LAMP_BRI_MAX) {
      lampEngineSetBrightness(parsed.brightness);
    }
    lampEngineSetCompatFx(parsed.fx, parsed.speed, parsed.intensity);
    accepted(sequence, fromShow);
  }
}

void lampProtocolLocalIgnite() {
  if (lampNodeState() == SHOWDUINO_LAMP_ST_EMERGENCY) {
    Serial.println("[LAMP] Ignition blocked — emergency");
    return;
  }
  if (lampNodeStateShowControlled()) {
    Serial.println("[LAMP] Ignition blocked — P4 show control");
    return;
  }
  if (lampEngineFlameLit()) {
    Serial.println("[LAMP] Local striker: already lit");
    return;
  }
  Serial.println("[LAMP] Local striker → IGNITE");
  lampProtocolApply("LAMP:IGNITE", 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
}

void lampProtocolAnnounce() {
  char line[96];
  formatAnnounce(line, sizeof(line));
  lampEspNowSend(line, 0);
}

void lampProtocolService() {
  lampEspNowService();

  const ShowduinoBlowClass blow = lampSensorsTakeBlowEvent();
  if (blow == SHOWDUINO_BLOW_SUSTAINED && lampEngineFlameLit() &&
      lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY) {
    lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_BLOW);
    Serial.println("[LAMP] Sustained blow → EXTINGUISH");
  } else if (blow == SHOWDUINO_BLOW_PUFF && lampEngineFlameLit() &&
             lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY) {
    lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_PUFF);
  } else if (blow == SHOWDUINO_BLOW_RELEASE &&
             lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY) {
    lampEngineApplyEvent(SHOWDUINO_CARBIDE_EV_BLOW_END);
  }

  if (showduino_lamp_comms_loss_extinguish(
          lampNodeState(),
          lampNodeStateShowControlled() ? 1 : 0,
          lampNodeStateAuthorityFresh(SHOWDUINO_LAMP_COMMS_TIMEOUT_MS) ? 1 : 0) &&
      lampNodeState() != SHOWDUINO_LAMP_ST_EMERGENCY) {
    Serial.println("[LAMP] Show-control comms timeout — extinguish");
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

  if (sApDueMs && (int32_t)(millis() - sApDueMs) >= 0) {
    sApDueMs = 0;
    lampWebEnsure();
  }
  lampWebService();

  const uint32_t announceMs =
      (lampNodeState() == SHOWDUINO_LAMP_ST_SHOW_CONTROLLED)
          ? SHOWDUINO_LAMP_ANNOUNCE_MS
          : SHOWDUINO_LAMP_ANNOUNCE_SEARCH_MS;
  if ((millis() - sLastAnnounce) >= announceMs) {
    sLastAnnounce = millis();
    lampProtocolAnnounce();
  }
}
