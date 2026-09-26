#include "MosfetProtocol.h"
#include "MosfetIdentity.h"
#include "MosfetNodeState.h"
#include "MosfetOutputEngine.h"
#include "EspNowMosfetTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_mosfet_node.h"
#include <Arduino.h>

static uint32_t sAnnounceMs = 0;
static uint32_t sLocalTestUntil = 0;

extern bool mosfetEspNowSend(const char *command, uint32_t sequence);

void mosfetProtocolBegin() {
  sAnnounceMs = 0;
  sLocalTestUntil = 0;
}

void mosfetProtocolAnnounce() {
  char mac[24];
  mosfetEspNowMacString(mac, sizeof(mac));
  char line[160];
  showduino_mosfet_format_announce(
      line, sizeof(line), mac, SHOWDUINO_MOSFET_NODE_FW,
      showduino_mosfet_state_name(mosfetNodeStateGet()),
      mosfetIdentityId(), mosfetIdentityName());
  mosfetEspNowSend(line, 0);
  Serial.printf("[MOSFET] %s\n", line);
}

void mosfetProtocolLoop() {
  const uint32_t now = millis();
  if (sLocalTestUntil && (int32_t)(now - sLocalTestUntil) >= 0) {
    mosfetOutputEngineAllOff("LOCAL_TEST_END");
    sLocalTestUntil = 0;
  }
  if (!sAnnounceMs || (now - sAnnounceMs) > 5000UL) {
    sAnnounceMs = now;
    mosfetProtocolAnnounce();
  }
}

void mosfetProtocolLocalTest() {
  if (mosfetNodeStateOwned() || mosfetNodeStateEmergency()) return;
  mosfetOutputEnginePulse(1, 100, SHOWDUINO_MOSFET_LOCAL_TEST_MS);
  sLocalTestUntil = millis() + SHOWDUINO_MOSFET_LOCAL_TEST_MS;
}

static void replyStatus(uint32_t sequence) {
  uint8_t lv[4];
  mosfetOutputEngineLevels(lv);
  char line[120];
  showduino_mosfet_format_status(line, sizeof(line), mosfetIdentityId(),
                                 mosfetNodeStateOwned() ? 1 : 0,
                                 mosfetNodeStateEmergency() ? 1 : 0, lv);
  mosfetEspNowSend(line, sequence);
  Serial.printf("[MOSFET] %s\n", line);
}

bool mosfetProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin) {
  if (!command || !command[0]) return false;
  ShowduinoMosfetCmd cls = showduino_mosfet_classify_command(command);
  ShowduinoMosfetFail gate = showduino_mosfet_can_accept(mosfetNodeStateGet(), cls, origin);
  if (gate != SHOWDUINO_MOSFET_FAIL_NONE) {
    char rej[96];
    snprintf(rej, sizeof(rej), "MOSFET:REJECTED:%s", showduino_mosfet_fail_name(gate));
    mosfetEspNowSend(rej, sequence);
    Serial.printf("[MOSFET] %s cmd=%s\n", rej, command);
    return false;
  }

  if (cls == SHOWDUINO_MOSFET_CMD_OWN_GRANT) {
    mosfetNodeStateOnGrant(millis());
    replyStatus(sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_EMERGENCY_STOP) {
    mosfetNodeStateOnEmergencyStop();
    replyStatus(sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_EMERGENCY_CLEAR) {
    mosfetNodeStateOnEmergencyClear();
    replyStatus(sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_STATUS) {
    replyStatus(sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_CAPS) {
    char caps[96];
    snprintf(caps, sizeof(caps), "MOSFET:CAPS:%s", SHOWDUINO_MOSFET_CAPS);
    mosfetEspNowSend(caps, sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_ALL_OFF) {
    mosfetOutputEngineAllOff("CMD");
    replyStatus(sequence);
    return true;
  }
  if (cls == SHOWDUINO_MOSFET_CMD_TEST) {
    mosfetProtocolLocalTest();
    replyStatus(sequence);
    return true;
  }

  ShowduinoMosfetOutCmd parsed = {};
  if (!showduino_mosfet_parse_out_command(command, &parsed)) {
    mosfetEspNowSend("MOSFET:REJECTED:BAD_COMMAND", sequence);
    return false;
  }
  if (parsed.cmd == SHOWDUINO_MOSFET_CMD_ID) {
    const char *cmd = showduino_mosfet_strip_node_prefix(command, nullptr, 0);
    if (cmd && strncmp(cmd, "MOSFET:", 7) == 0) cmd += 7;
    if (cmd && strncmp(cmd, "ID:", 3) == 0) mosfetIdentitySetId(cmd + 3);
    replyStatus(sequence);
    return true;
  }
  if (parsed.cmd == SHOWDUINO_MOSFET_CMD_NAME) {
    const char *cmd = showduino_mosfet_strip_node_prefix(command, nullptr, 0);
    if (cmd && strncmp(cmd, "MOSFET:", 7) == 0) cmd += 7;
    if (cmd && strncmp(cmd, "NAME:", 5) == 0) mosfetIdentitySetName(cmd + 5);
    replyStatus(sequence);
    return true;
  }
  if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_NAME) {
    mosfetIdentitySetOutName(parsed.channel, parsed.name);
    replyStatus(sequence);
    return true;
  }
  if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_OFF) {
    mosfetOutputEngineOff(parsed.channel);
  } else if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_ON) {
    mosfetOutputEngineOn(parsed.channel);
  } else if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_LEVEL) {
    mosfetOutputEngineSetLevel(parsed.channel, parsed.level);
  } else if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_PULSE) {
    mosfetOutputEnginePulse(parsed.channel, parsed.level, parsed.durationMs);
  } else if (parsed.cmd == SHOWDUINO_MOSFET_CMD_OUT_FADE) {
    mosfetOutputEngineFade(parsed.channel, parsed.level, parsed.durationMs);
  }
  replyStatus(sequence);
  return true;
}
