#include "EmergencyProtocol.h"
#include "EmergencyIdentity.h"
#include "EmergencyInput.h"
#include "EmergencyWeb.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

ShowduinoEmergencyMachine gEmergencyMachine;

static uint32_t sLastAnnounce = 0;
static uint32_t sApDueMs = 0;
static uint32_t sSeq = 1;
static uint8_t sLastRadio = 0;

static void requestSoftAp() {
  if (!sApDueMs) sApDueMs = millis() + 400UL;
}

static void sendLine(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  if (!strncmp(line, "ANNOUNCE:", 9) || !strncmp(line, "STATUS:", 7)) {
    SD_LOGT("ESTOP", "%s", line);
  } else if (showduino_emergency_is_assert(line)) {
    if (showduino_emergency_should_log_assert(&gEmergencyMachine, millis())) {
      SD_LOGI("ESTOP", "EMERGENCY ASSERT SOURCE=%s NAME=%s",
              emergencyIdentityId(), emergencyIdentityName());
    } else {
      SD_LOGT("ESTOP", "%s", line);
    }
  } else {
    SD_LOGI("ESTOP", "%s", line);
  }
  emergencyEspNowSend(line, seq);
}

static void formatAnnounce(char *out, size_t n) {
  ShowduinoEmergencyAnnounce an{};
  emergencyEspNowMacString(an.mac, sizeof(an.mac));
  strncpy(an.firmware, SHOWDUINO_EMERGENCY_NODE_FW, sizeof(an.firmware) - 1);
  strncpy(an.state, showduino_emergency_state_name(gEmergencyMachine.state),
          sizeof(an.state) - 1);
  strncpy(an.id, emergencyIdentityId(), sizeof(an.id) - 1);
  strncpy(an.name, emergencyIdentityName(), sizeof(an.name) - 1);
  an.input_open = gEmergencyMachine.input_open;
  an.latched = gEmergencyMachine.latched;
  an.acked = gEmergencyMachine.acked;
  showduino_emergency_format_announce(&an, out, n);
}

static void sendStatus(uint32_t seq) {
  char line[96];
  snprintf(line, sizeof(line), "STATUS:%s:IN=%s:L=%u:A=%u:%s",
           showduino_emergency_state_name(gEmergencyMachine.state),
           showduino_emergency_button_name(gEmergencyMachine.input_open),
           (unsigned)gEmergencyMachine.latched,
           (unsigned)gEmergencyMachine.acked,
           emergencyIdentityId());
  sendLine(line, seq);
}

void emergencyProtocolBegin() {
  sLastAnnounce = 0;
  requestSoftAp();
}

void emergencyProtocolAnnounce() {
  char line[SHOWDUINO_NODE_COMMAND_MAX];
  formatAnnounce(line, sizeof(line));
  sendLine(line, 0);
}

void emergencyProtocolTryLocalRearm() {
  if (showduino_emergency_machine_rearm(&gEmergencyMachine)) {
    SD_LOGI("ESTOP", "MAINTENANCE LOCAL RESET — station NORMAL. P4 emergency unchanged.");
    emergencyProtocolAnnounce();
  }
}

void emergencyProtocolApply(const char *command, uint32_t sequence) {
  if (!command || !command[0]) return;
  if (emergencyEspNowHaveComms()) {
    showduino_emergency_machine_set_radio(&gEmergencyMachine, 1);
  }

  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  const char *work = command;
  if (!strncmp(command, "ESTOP:NODE:", 11) || !strncmp(command, "EMERGENCY:NODE:", 15)) {
    const char *p = strchr(command + 5, ':');
    if (p && !strncmp(p, ":NODE:", 6)) p += 6;
    else if (!strncmp(command, "ESTOP:NODE:", 11)) p = command + 11;
    else p = command + 15;
    const char *c = strchr(p, ':');
    if (c) {
      size_t n = (size_t)(c - p);
      if (n > SHOWDUINO_EMERGENCY_ID_MAX) n = SHOWDUINO_EMERGENCY_ID_MAX;
      memcpy(id, p, n);
      id[n] = 0;
      work = c + 1;
      if (id[0] && !showduino_emergency_id_equal(id, emergencyIdentityId())) return;
    }
  }

  if (showduino_emergency_is_clear_token(work) &&
      strcmp(work, "EMERGENCY:CLEAR") != 0) {
    SD_LOGW("ESTOP", "REJECTED local/remote clear token: %s", work);
    sendLine("ESTOP:REJECTED:NO_CLEAR", sequence);
    return;
  }

  const ShowduinoEmergencyNodeCmd cmd = showduino_emergency_parse_command(work);
  if (cmd == SHOWDUINO_ESTOP_CMD_REJECT_CLEAR) {
    sendLine("ESTOP:REJECTED:NO_CLEAR", sequence);
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_ACK_LATCHED) {
    showduino_emergency_machine_ack(&gEmergencyMachine);
    SD_LOGI("ESTOP", "P4 ACK LATCHED — not a clear");
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED) {
    showduino_emergency_machine_global_observed(&gEmergencyMachine);
    if (showduino_emergency_button_pressed(&gEmergencyMachine)) {
      SD_LOGI("ESTOP", "GLOBAL CLEAR OBSERVED — button still PRESSED, re-asserting");
    } else {
      SD_LOGI("ESTOP", "GLOBAL CLEAR OBSERVED — button RELEASED, local NORMAL");
    }
    emergencyProtocolAnnounce();
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_OWN_GRANT) {
    requestSoftAp();
    sendLine("ESTOP:OWNED", sequence);
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_STATUS) {
    sendStatus(sequence);
    emergencyProtocolAnnounce();
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_REARM) {
    emergencyProtocolTryLocalRearm();
    sendStatus(sequence);
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_ID) {
    const char *v = strstr(work, "ID:");
    if (!v || !emergencyIdentitySetId(v + 3)) {
      sendLine("ESTOP:ERROR:ID", sequence);
      return;
    }
    SD_LOGI("ESTOP", "NODE ID -> %s", emergencyIdentityId());
    emergencyProtocolAnnounce();
    return;
  }
  if (cmd == SHOWDUINO_ESTOP_CMD_NAME) {
    const char *v = strstr(work, "NAME:");
    if (!v || !emergencyIdentitySetName(v + 5)) {
      sendLine("ESTOP:ERROR:NAME", sequence);
      return;
    }
    SD_LOGI("ESTOP", "NAME -> %s", emergencyIdentityName());
    emergencyProtocolAnnounce();
    return;
  }
}

void emergencyProtocolService() {
  emergencyInputService();
  showduino_emergency_machine_input(&gEmergencyMachine, emergencyInputPressed());

  const int radio = emergencyEspNowHaveComms() && emergencyEspNowReady() ? 1 : 0;
  if ((uint8_t)radio != sLastRadio) {
    showduino_emergency_machine_set_radio(&gEmergencyMachine, radio);
    sLastRadio = (uint8_t)radio;
  } else {
    showduino_emergency_machine_set_radio(&gEmergencyMachine, radio);
  }
  emergencyEspNowService();

  const uint32_t now = millis();
  if (emergencyInputRearmHeld(now)) {
    emergencyProtocolTryLocalRearm();
  }

  if (showduino_emergency_machine_want_tx(&gEmergencyMachine, now)) {
    char cmd[48];
    showduino_emergency_format_assert(emergencyIdentityId(), emergencyIdentityName(),
                                      cmd, sizeof(cmd));
    sendLine(cmd, sSeq++);
    showduino_emergency_machine_sent(&gEmergencyMachine, now);
  }

  if (sApDueMs && (int32_t)(now - sApDueMs) >= 0) {
    sApDueMs = 0;
    emergencyWebEnsure();
  }
  emergencyWebService();

  const uint32_t announceMs = radio ? SHOWDUINO_EMERGENCY_HEARTBEAT_MS
                                    : SHOWDUINO_EMERGENCY_SEARCH_MS;
  if ((now - sLastAnnounce) >= announceMs) {
    sLastAnnounce = now;
    emergencyProtocolAnnounce();
  }
}

ShowduinoEmergencyNodeState emergencyProtocolState() {
  return gEmergencyMachine.state;
}
