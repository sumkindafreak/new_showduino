#include "PixelProtocol.h"
#include "PixelEngine.h"
#include "PixelNodeState.h"
#include "PixelIdentity.h"
#include "PixelWeb.h"
#include "EspNowPixelTransport.h"
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
  pixelNodeStateSetLastResult(line);
  pixelEspNowSend(line, seq);
}

void pixelProtocolFormatStatus(char *out, size_t n) {
  snprintf(out, n, "STATUS:%s:P%u:I%u:S%u:%s",
           pixelNodeStateName(),
           (unsigned)pixelEngineConfiguredCount(),
           pixelEngineReady() ? 1U : 0U,
           (unsigned)pixelEngineActiveSegments(),
           pixelNodeStateFault());
}

static void formatAnnounce(char *out, size_t n) {
  char mac[24];
  pixelEspNowMacString(mac, sizeof(mac));
  showduino_pixel_format_announce(out, n, mac, SHOWDUINO_PIXEL_NODE_FW,
                                  pixelNodeStateName(), pixelIdentityId(),
                                  pixelIdentityName(),
                                  pixelEngineConfiguredCount(),
                                  pixelEngineReady() ? 1 : 0,
                                  pixelEngineActiveSegments());
}

static void failSafeBlackout(const char *why) {
  Serial.printf("[PIXEL] Fail-safe blackout — %s\n", why ? why : "authority lost");
  pixelEngineBlackout();
}

static void onOwnerEdges() {
  if (pixelOwnerEnteredShow()) {
    SD_LOGI("PIXEL", "NODE GRANTED");
    failSafeBlackout("P4 GRANT — local/web tests cleared");
    requestSoftAp();
  }
  if (pixelOwnerLostAuthority()) {
    SD_LOGW("PIXEL", "COMMS LOST");
    failSafeBlackout("P4 authority lost — SHOWDUINO_FAILSAFE_PIXEL_BLACKOUT");
    requestSoftAp();
  } else if (pixelOwnerEnteredStandalone()) {
    SD_LOGI("PIXEL", "Control -> STANDALONE");
    requestSoftAp();
  }
}

void pixelProtocolBegin() {
  sLastAnnounce = 0;
  sActiveSeq = 0;
  requestSoftAp();
}

void pixelProtocolAnnounce() {
  char line[SHOWDUINO_NODE_COMMAND_MAX];
  formatAnnounce(line, sizeof(line));
  pixelEspNowSend(line, 0);
}

void pixelProtocolApply(const char *command, uint32_t sequence, ShowduinoCmdOrigin origin) {
  if (!command || !command[0]) return;
  pixelNodeStateSetLastCommand(command);
  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW) pixelNodeStateNoteComms();

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
    if (id[0] && !showduino_pixel_id_equal(id, pixelIdentityId())) {
      return;
    }
    work = inner;
  }

  const ShowduinoPixelCmd cmd = showduino_pixel_classify_command(work);
  const bool initialised = pixelEngineReady();
  ShowduinoPixelNodeState st = pixelNodeStateDisplay();
  if (pixelEngineEmergency()) st = SHOWDUINO_PIXEL_ST_EMERGENCY;

  if (origin == SHOWDUINO_CMD_ORIGIN_SHOW &&
      (cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT || cmd == SHOWDUINO_PIXEL_CMD_STATUS ||
       showduino_pixel_cmd_theatrical(cmd))) {
    pixelOwnerApplyEvent(cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT
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
    pixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_EMERGENCY_STOP);
    pixelEngineOnEmergency(true);
    pixelNodeStateSet(SHOWDUINO_PIXEL_ST_EMERGENCY);
    SD_LOGI("PIXEL", "EMERGENCY ACTIVE");
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:EMERGENCY:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR) {
    pixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR);
    onOwnerEdges();
    pixelEngineOnEmergency(false);
    pixelEngineBlackout();
    SD_LOGI("PIXEL", "EMERGENCY CLEARED");
    if (pixelNodeState() != SHOWDUINO_PIXEL_ST_FAULT) {
      pixelNodeStateSet(SHOWDUINO_PIXEL_ST_SEARCHING);
    }
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_ID) {
    const char *v = strstr(work, "ID:");
    if (!v || !pixelIdentitySetId(v + 3)) {
      report("PIXEL:ERROR:ID", sequence);
      return;
    }
    SD_LOGI("PIXEL", "NODE ID -> %s", pixelIdentityId());
    char line[48];
    snprintf(line, sizeof(line), "PIXEL:ID:OK:%s", pixelIdentityId());
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_NAME) {
    const char *v = strstr(work, "NAME:");
    if (!v || !pixelIdentitySetName(v + 5)) {
      report("PIXEL:ERROR:NAME", sequence);
      return;
    }
    SD_LOGI("PIXEL", "NAME -> %s", pixelIdentityName());
    char line[64];
    snprintf(line, sizeof(line), "PIXEL:NAME:OK:%s", pixelIdentityName());
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_STATUS) {
    char line[96];
    pixelProtocolFormatStatus(line, sizeof(line));
    report(line, sequence);
    char caps[96];
    snprintf(caps, sizeof(caps), "PIXEL:CAPS:%s", SHOWDUINO_PIXEL_CAPS);
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
  if (pixelEngineHandleCommand(work, reply, sizeof(reply))) {
    if (reply[0]) report(reply, sequence);
    sActiveSeq = sequence;
    return;
  }

  char line[64];
  snprintf(line, sizeof(line), "PIXEL:FAILED:%lu:BAD_COMMAND", (unsigned long)sequence);
  report(line, sequence);
}

void pixelProtocolLocalTest() {
  if (pixelEngineEmergency()) {
    Serial.println("[PIXEL] Local test blocked — emergency");
    return;
  }
  if (pixelNodeStateShowControlled()) {
    Serial.println("[PIXEL] Local test blocked — P4 show control");
    return;
  }
  if (!pixelEngineReady()) {
    Serial.println("[PIXEL] Local test blocked — NOT INITIALISED");
    return;
  }
  if (pixelEngineLocateActive()) {
    pixelProtocolApply("PIXEL:BLACKOUT", 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
    return;
  }
  pixelProtocolApply("PIXEL:TEST", 0, SHOWDUINO_CMD_ORIGIN_LOCAL);
}

void pixelProtocolService() {
  pixelOwnerTick();
  onOwnerEdges();
  pixelEspNowService();

  if (sApDueMs && (int32_t)(millis() - sApDueMs) >= 0) {
    sApDueMs = 0;
    pixelWebEnsure();
  }
  pixelWebService();

  const uint32_t announceMs =
      (pixelOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED)
          ? SHOWDUINO_PIXEL_ANNOUNCE_MS
          : SHOWDUINO_PIXEL_ANNOUNCE_SEARCH_MS;
  if ((millis() - sLastAnnounce) >= announceMs) {
    sLastAnnounce = millis();
    pixelProtocolAnnounce();
  }
  (void)sActiveSeq;
}
