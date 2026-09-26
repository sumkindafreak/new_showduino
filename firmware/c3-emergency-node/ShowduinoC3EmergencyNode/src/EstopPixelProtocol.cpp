#include "EstopPixelProtocol.h"
#include "EstopPixelEngine.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_pixel_node.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

/*
 * Pixel capability on the Emergency Node peer (ESTOP-xx).
 * Accepts PIXEL:* and ESTOP:NODE:PIXEL:* — never invents LED-xx identity.
 */

static bool sShowOwned = false;
static bool sHadRadio = false;
static uint8_t sLastLatchPixel = 0xFF;

static void report(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  if (!strncmp(line, "PIXEL:STATUS:", 13) || !strncmp(line, "PIXEL:CAPS:", 11)) {
    SD_LOGT("ESTOP-PIX", "%s", line);
  } else if (strstr(line, "REJECTED") || strstr(line, "ERROR") || strstr(line, "FAILED")) {
    SD_LOGW("ESTOP-PIX", "%s", line);
  } else {
    SD_LOGI("ESTOP-PIX", "%s", line);
  }
  emergencyEspNowSend(line, seq);
}

void estopPixelProtocolBegin() {
  sShowOwned = false;
  sHadRadio = emergencyEspNowHaveComms();
  sLastLatchPixel = 0xFF;
}

bool estopPixelShowControlled() { return sShowOwned; }

void estopPixelProtocolOnEmergencyLatch(bool latched) {
  /* Assert path must already have queued/sent ESTOP before this runs. */
  if (latched) {
    pixelEngineOnEmergency(true);
  }
}

void estopPixelProtocolOnGlobalClearNormal() {
  /* Legitimate clear + button released: BLACK, do not auto-resume FX. */
  pixelEngineOnEmergency(false);
  pixelEngineBlackout();
}

void estopPixelProtocolOnRadioLost() {
  if (pixelEngineEmergency()) return; /* Emergency White stays authoritative */
  if (sShowOwned || pixelEngineReady()) {
    Serial.println("[ESTOP-PIX] Fail-safe blackout — radio/authority lost");
    pixelEngineBlackout();
  }
  sShowOwned = false;
}

void estopPixelProtocolOnRadioRestored() {
  /* Ownership re-granted via ESTOP:OWN:GRANT / PIXEL:OWN:GRANT. */
}

bool estopPixelProtocolApply(const char *command, uint32_t sequence) {
  if (!command || !command[0]) return false;

  const char *work = command;
  if (!strncmp(command, "ESTOP:NODE:PIXEL:", 17)) {
    work = command + 17;
  } else if (!strncmp(command, "EMERGENCY:NODE:PIXEL:", 21)) {
    work = command + 21;
  } else if (!strncmp(command, "PIXEL:", 6)) {
    work = command;
  } else if (!strcmp(command, "PIXEL:OWN:GRANT") || !strcmp(command, "OWN:GRANT")) {
    work = "PIXEL:OWN:GRANT";
  } else {
    return false;
  }

  char innerBuf[SHOWDUINO_NODE_COMMAND_MAX];
  if (strncmp(work, "PIXEL:", 6) != 0 && strncmp(work, "EMERGENCY:", 10) != 0 &&
      strcmp(work, "OWN:GRANT") != 0) {
    snprintf(innerBuf, sizeof(innerBuf), "PIXEL:%s", work);
    work = innerBuf;
  } else if (!strcmp(work, "OWN:GRANT")) {
    work = "PIXEL:OWN:GRANT";
  }

  const ShowduinoPixelCmd cmd = showduino_pixel_classify_command(work);

  if (cmd == SHOWDUINO_PIXEL_CMD_OWN_GRANT) {
    const bool entered = !sShowOwned;
    sShowOwned = true;
    if (entered) {
      /* First grant clears local/web tests — keepalive grants must not flash black. */
      pixelEngineBlackout();
    }
    report("PIXEL:OWNED", sequence);
    char caps[96];
    snprintf(caps, sizeof(caps), "PIXEL:CAPS:%s", SHOWDUINO_PIXEL_CAPS);
    report(caps, sequence);
    return true;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_STOP) {
    pixelEngineOnEmergency(true);
    report("PIXEL:EMERGENCY", sequence);
    return true;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_EMERGENCY_CLEAR) {
    /* P4 clear token for pixel engine only — Emergency latch is separate. */
    if (!pixelEngineEmergency()) {
      pixelEngineBlackout();
    }
    report("PIXEL:IDLE", sequence);
    return true;
  }

  if (cmd == SHOWDUINO_PIXEL_CMD_STATUS) {
    char reply[SHOWDUINO_NODE_COMMAND_MAX];
    pixelEngineHandleCommand("PIXEL:STATUS", reply, sizeof(reply));
    if (reply[0]) report(reply, sequence);
    char caps[96];
    snprintf(caps, sizeof(caps), "PIXEL:CAPS:%s", SHOWDUINO_PIXEL_CAPS);
    report(caps, sequence);
    return true;
  }

  /* Local Emergency latch always wins over theatrical Pixel commands. */
  if (pixelEngineEmergency() && showduino_pixel_cmd_theatrical(cmd) &&
      cmd != SHOWDUINO_PIXEL_CMD_OFF) {
    report("PIXEL:REJECTED:EMERGENCY_ACTIVE", sequence);
    return true;
  }

  char reply[SHOWDUINO_NODE_COMMAND_MAX];
  if (pixelEngineHandleCommand(work, reply, sizeof(reply))) {
    if (reply[0]) report(reply, sequence);
    return true;
  }

  report("PIXEL:FAILED:BAD_COMMAND", sequence);
  return true;
}

void estopPixelProtocolService() {
  const bool radio = emergencyEspNowHaveComms();
  if (sHadRadio && !radio) {
    estopPixelProtocolOnRadioLost();
  } else if (!sHadRadio && radio) {
    estopPixelProtocolOnRadioRestored();
  }
  sHadRadio = radio;

  /* Service renderer after Emergency input/assert path (caller order). */
  pixelEngineService();
  (void)sLastLatchPixel;
}
