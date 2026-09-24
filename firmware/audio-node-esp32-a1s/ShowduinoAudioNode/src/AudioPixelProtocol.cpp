#include "AudioPixelProtocol.h"
#include "AudioPixelEngine.h"
#include "AudioPixelNodeState.h"
#include "AudioPixelIdentity.h"

#include "EspNowNodeTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_pixel_node.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

static uint32_t sLastAnnounce = 0;
static uint32_t sActiveSeq = 0;
static uint32_t sApDueMs = 0;

static void requestSoftAp() {
  if (!sApDueMs) sApDueMs = millis() + 400UL;
}

static bool isRoutine(const char *line) {
  if (!line) return true;
  if (!strncmp(line, "ANNOUNCE:", 9)) return true;
  if (!strncmp(line, "STATUS:", 7)) return true;
  if (!strncmp(line, "PIXEL:OWNED:", 12)) return true;
  if (!strncmp(line, "PIXEL:CAPS:", 11)) return true;
  return false;
}

static void report(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  if (isRoutine(line)) {
    SD_LOGT("PIXEL", "%s", line);
  } else if (strstr(line, "EMERGENCY") || strstr(line, "FAILED") || strstr(line, "FAULT")) {
    SD_LOGW("PIXEL", "%s", line);
  } else {
    SD_LOGI("PIXEL", "%s", line);
  }
  audioPixelNodeStateSetLastResult(line);
  audioEspNowSend(line, seq);
}

void audioPixelProtocolFormatStatus(char *out, size_t n) {
  snprintf(out, n, "STATUS:%s:P%u:I%u:S%u:%s",
           audioPixelNodeStateName(),
           (unsigned)audioPixelEngineConfiguredCount(),
           audioPixelEngineReady() ? 1U : 0U,
           (unsigned)audioPixelEngineActiveSegments(),
           audioPixelNodeStateFault());
}

static void formatAnnounce(char *out, size_t n) {
  char mac[24];
  audioEspNowMacString(mac, sizeof(mac));
  showduino_pixel_format_announce(out, n, mac, SHOWDUINO_AUDIO_NODE_FW,
                                  audioPixelNodeStateName(), audioPixelIdentityId(),
                                  audioPixelIdentityName(),
                                  audioPixelEngineConfiguredCount(),
                                  audioPixelEngineReady() ? 1 : 0,
                                  audioPixelEngineActiveSegments());
}

static void failSafeBlackout(const char *why) {
  Serial.printf("[PIXEL] Fail-safe blackout — %s\n", why ? why : "authority lost");
  audioPixelEngineBlackout();
}

static void onOwnerEdges() {
  if (audioPixelOwnerEnteredShow()) {
    SD_LOGI("PIXEL", "NODE GRANTED");
    failSafeBlackout("P4 GRANT — local/web tests cleared");
    requestSoftAp();
  }
  if (audioPixelOwnerLostAuthority()) {
    SD_LOGW("PIXEL", "COMMS LOST");
    failSafeBlackout("P4 authority lost — SHOWDUINO_FAILSAFE_PIXEL_BLACKOUT");
    requestSoftAp();
  } else if (audioPixelOwnerEnteredStandalone()) {
    SD_LOGI("PIXEL", "Control -> STANDALONE");
    requestSoftAp();
  }
}

void audioPixelProtocolBegin() {
  sLastAnnounce = 0;
  sActiveSeq = 0;
  requestSoftAp();
}

void audioPixelProtocolAnnounce() {
  char line[SHOWDUINO_NODE_COMMAND_MAX];
  formatAnnounce(line, sizeof(line));
  audioEspNowSend(line, 0);
}

void audioPixelProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin) {
  if (!command || !command[0]) return;
  audioPixelNodeStateSetLastCommand(command);
  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) audioPixelNodeStateNoteComms();

  char id[SHOWDUINO_PIXEL_ID_MAX + 1] = "";
  char inner[SHOWDUINO_NODE_COMMAND_MAX];
  const char *work = command;
  if (!strncmp(command, "PIXEL:NODE:", 11)) {
    if (!showduino_pixel_inner_command(command, id, sizeof(id), inner, sizeof(inner))) {
      char line[64];
      snprintf(line, sizeof(line), "PIXEL:FAILED:%lu:BAD_ID", (unsigned long)sequence);
      report(line, sequence);
      return;
    }
    if (id[0] && !showduino_pixel_id_equal(id, audioPixelIdentityId())) {
      return;
    }
    work = inner;
  }

  const ShowduinoPixelCmd cmd = showduino_pixel_classify_command(work);
  const bool initialised = audioPixelEngineReady();
  ShowduinoPixelNodeState st = audioPixelNodeStateDisplay();
  if (audioPixelEngineEmergency()) st = SHOWDUINO_PIXEL_ST_EMERGENCY;

  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW &&
      (cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT || cmd == SHOWDUINO_PIXEL_CMD_STATUS ||
       showduino_pixel_cmd_theatrical(cmd))) {
    audioPixelOwnerApplyEvent(cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT
                             ? SHOWDUINO_OWNER_EV_GRANT
                             : SHOWDUINO_OWNER_EV_KEEP);
  }

  const ShowduinoPixelFail gate =
      showduino_pixel_can_accept_ex(st, cmd, origin, initialised ? 1 : 0);
  if (gate != SHOWDUINO_PIXEL_FAIL_NONE) {
    char line[64];
    snprintf(line, sizeof(line), "PIXEL:FAILED:%lu:%s",
             (unsigned long)sequence, showduino_pixel_fail_name(gate));
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT) {
    onOwnerEdges();
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:OWNED:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP) {
    audioPixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_EMERGENCY_STOP);
    audioPixelEngineOnEmergency(true);
    audioPixelNodeStateSet(SHOWDUINO_PIXEL_ST_EMERGENCY);
    SD_LOGI("PIXEL", "EMERGENCY ACTIVE");
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:EMERGENCY:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR) {
    audioPixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR);
    onOwnerEdges();
    audioPixelEngineOnEmergency(false);
    audioPixelEngineBlackout();
    SD_LOGI("PIXEL", "EMERGENCY CLEARED");
    if (audioPixelNodeState() != SHOWDUINO_PIXEL_ST_FAULT) {
      audioPixelNodeStateSet(SHOWDUINO_PIXEL_ST_SEARCHING);
    }
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_ID) {
    const char *v = strstr(work, "ID:");
    if (!v || !audioPixelIdentitySetId(v + 3)) {
      report("PIXEL:ERROR:ID", sequence);
      return;
    }
    SD_LOGI("PIXEL", "NODE ID -> %s", audioPixelIdentityId());
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:ID:OK:%s", audioPixelIdentityId());
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_NAME) {
    const char *v = strstr(work, "NAME:");
    if (!v || !audioPixelIdentitySetName(v + 5)) {
      report("PIXEL:ERROR:NAME", sequence);
      return;
    }
    SD_LOGI("PIXEL", "NAME -> %s", audioPixelIdentityName());
    char line[64];
    snprintf(line, sizeof(line), "PIXEL:NAME:OK:%s", audioPixelIdentityName());
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_STATUS) {
    char line[96];
    audioPixelProtocolFormatStatus(line, sizeof(line));
    report(line, sequence);
    char caps[96];
    snprintf(caps, sizeof(caps), "PIXEL:CAPS:%s", SHOWDUINO_AUDIO_PIXEL_CAPS);
    report(caps, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_LOCATE) {
    SD_LOGI("PIXEL", "LOCATE");
  }
  if (cmd == SHOWDUINO_PIXEL_CMD_COUNT) {
    SD_LOGI("PIXEL", "PIXEL COUNT SAVED");
  }
  if (cmd == SHOWDUINO_PIXEL_CMD_INIT) {
    SD_LOGI("PIXEL", "PIXEL LINE INITIALISED");
  }
  if (cmd == SHOWDUINO_PIXEL_CMD_SEGMENT) {
    SD_LOGI("PIXEL", "SEGMENT COMMAND");
  }

  char reply[SHOWDUINO_NODE_COMMAND_MAX];
  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) {
    SD_LOGI("PIXEL", "RX: %s", work);
  }
  if (audioPixelEngineHandleCommand(work, reply, sizeof(reply))) {
    if (reply[0]) report(reply, sequence);
    sActiveSeq = sequence;
    return;
  }

  char line[64];
  snprintf(line, sizeof(line), "PIXEL:FAILED:%lu:BAD_COMMAND", (unsigned long)sequence);
  report(line, sequence);
}

void audioPixelProtocolLocalTest() {
  if (audioPixelEngineEmergency()) {
    Serial.println("[PIXEL] Local test blocked — emergency");
    return;
  }
  if (audioPixelNodeStateShowControlled()) {
    Serial.println("[PIXEL] Local test blocked — P4 show control");
    return;
  }
  if (!audioPixelEngineReady()) {
    Serial.println("[PIXEL] Local test blocked — NOT INITIALISED");
    return;
  }
  if (audioPixelEngineLocateActive()) {
    audioPixelProtocolApply("PIXEL:BLACKOUT", 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
    return;
  }
  audioPixelProtocolApply("PIXEL:TEST", 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
}

void audioPixelProtocolService() {
  audioPixelOwnerTick();
  onOwnerEdges();
  audioEspNowService();

  if (sApDueMs && (int32_t)(millis() - sApDueMs) >= 0) {
    sApDueMs = 0;
  
  }


  const uint32_t announceMs =
      (audioPixelOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED)
          ? SHOWDUINO_AUDIO_ANNOUNCE_MS
          : SHOWDUINO_AUDIO_ANNOUNCE_SEARCH_MS;
  if ((millis() - sLastAnnounce) >= announceMs) {
    sLastAnnounce = millis();
    /* Audio Node discovery is authoritative; pixel capability is included in
       AUDIO:CAPS rather than advertising a second node identity. */
  }
  (void)sActiveSeq;
}
