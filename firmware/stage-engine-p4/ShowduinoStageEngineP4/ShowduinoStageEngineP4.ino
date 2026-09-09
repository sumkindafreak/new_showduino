/*
  Showduino Stage Engine - ESP32-P4

  Role:
  - Authoritative show engine on the Waveshare ESP32-P4-Module-DEV-KIT.
  - Receives Director commands via the dedicated ESP32-S3 Comms Controller UART.
  - Executes hardware actions locally and remains safe if the S3 is absent.

  Current features:
  - HELLO capability handshake
  - STATUS:REQUEST response
  - Latched EMERGENCY:STOP / EMERGENCY:CLEAR
  - Physical E-stop GPIO (when assigned in BoardConfig.h)
  - Emergency audio loop from SD
  - Dedicated emergency/signage NeoPixels on GPIO24
  - Segmented theatrical Show Pixel engine on GPIO23
  - SHOW: timeline runtime (ShowRuntimeOwner)
  - HEARTBEAT response
  - Local USB Serial maintenance console (same command dispatcher as Comms UART)
*/

#include <Arduino.h>
#include "BoardConfig.h"
#include "ShowEngineState.h"
#include "ShowRuntimeOwner.h"
#include "src/StageStorage.h"
#include "src/ProductionStore.h"
#include "src/StageAudio.h"
#include "src/EmergencyPixels.h"
#include "src/ShowPixels.h"
#include "src/EmergencyInput.h"
#include "src/WebApiHandler.h"
#include "src/plugin/PluginBus.h"
#include "src/StageDiagnostics.h"
#include "src/StageTime.h"
#include "src/network/ShowNetwork.h"
#include "src/e131/E131Receiver.h"
#include "src/storage/StageStore.h"
#include "src/storage/StageLog.h"
#include "src/storage/StageConfig.h"
#include "src/nodes/AudioNodeLink.h"
#include "src/nodes/LampNodeLink.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"

// -----------------------------
// Serial configuration
// -----------------------------
#define DEBUG_BAUD 115200

static void statusLedWrite(uint8_t level) {
#if SHOWDUINO_STATUS_LED_PIN >= 0
  digitalWrite(SHOWDUINO_STATUS_LED_PIN, level);
#else
  (void)level;
#endif
}

enum class EmergencySource : uint8_t {
  Remote = 0, /* Director command via Comms UART */
  Physical,
  LocalUsb     /* USB Serial maintenance console — not GPIO25 */
};

enum class CommandSource : uint8_t {
  Comms = 0,
  LocalUsb
};

// -----------------------------
// Runtime state
// -----------------------------
ShowEngineState gEngine;
ShowRuntimeOwner gRuntime;
ProductionStore gProductionStore;
static bool sProductionStoreReady = false;
static uint32_t sProductionStoreRetryMs = 0;

bool emergencyLocked = false;
uint8_t gEmergencySourceId = 0; /* 0 none, 1 director/comms, 2 physical, 3 local USB */
unsigned long lastHeartbeatMs = 0;

String inputBuffer = "";

static CommandSource sCmdSource = CommandSource::Comms;
static String *sWebReplySink = nullptr;
static char sUsbLine[SHOWDUINO_COMMS_CMD_MAX + 1];
static uint16_t sUsbLen = 0;
static bool sUsbOverflow = false;

void triggerEmergency(EmergencySource source);
void clearEmergencyStop();
void handleCommand(String command, CommandSource source);
void handleCommand(String command);
void readCommsSerial();
void readUsbSerial();
void serviceCommsLink();

static bool physicalEstopAssertedNow() {
  /* Debounced hold check. A single noisy sample must not block Director CLEAR
   * after a momentary press that has already been released. */
  return emergencyInputLoopOpen();
}

// -----------------------------
// Comms UART link (ESP32-S3 Comms Controller)
// -----------------------------
/* Peer ROM/bootloader text on the UART must not be treated as Director commands.
 * Replying ERR:UNKNOWN_COMMAND during peer boot can reset-loop the radio. */
static uint32_t sUartHushUntilMs = 0;
static uint16_t sUartNoiseCount = 0;
static bool sCommsUartReady = false;
static bool sCommsLinkUp = false;
static bool sCommsEverUp = false;
static bool sCommsSawDirector = false;
static uint32_t sCommsLastRxMs = 0;
static bool sDirectorPresent = false;
static uint32_t sDirectorLastDeskMs = 0;

static bool uartHushActive() {
  return (int32_t)(millis() - sUartHushUntilMs) < 0;
}

static bool isCoprocessorBootBanner(const String &c) {
  if (c.startsWith("ESP-ROM:")) return true;
  if (c.startsWith("Build:")) return true;
  if (c.startsWith("rst:")) return true;
  if (c.startsWith("Saved PC:")) return true;
  if (c.startsWith("SPIWP:")) return true;
  if (c.startsWith("mode:")) return true;
  if (c.startsWith("load:")) return true;
  if (c.startsWith("entry ")) return true;
  if (c.startsWith("ets ")) return true;
  if (c.startsWith("Guru Meditation")) return true;
  if (c.indexOf("SPI_FAST_FLASH_BOOT") >= 0) return true;
  if (c.indexOf("RTC_SW_SYS_RST") >= 0) return true;
  return false;
}

static bool isKnownCommsCommand(const String &c) {
  if (c == "HELLO" || c == "HEARTBEAT" || c == "STOP:ALL") return true;
  if (c == "PANIC" || c == "ESTOP" || c == "E-STOP") return true;
  if (c.startsWith("SHOW:") || c.startsWith("AUDIO:") || c.startsWith("EMERGENCY:")) return true;
  if (c.startsWith("STATUS:") || c.startsWith("TIME:") || c.startsWith("DMX:") ||
      c.startsWith("PIXEL:") || c.startsWith("NET:") || c.startsWith("E131:") ||
      c.startsWith("STORAGE:")) return true;
  if (c.startsWith("PLUGIN:")) return true;
  if (c.startsWith("PRODUCTION:") || c.startsWith("NODE:")) return true;
  if (c.startsWith("WEB/")) return true;
  if (c.startsWith("DIAG:")) return true;
  if (c.startsWith(SHOWDUINO_LEGACY_ROUTE_PREFIX) || c.startsWith("STATE:") ||
      c.startsWith("SNAPSHOT:") || c.startsWith("BOOT:") || c.startsWith("FW:") ||
      c.startsWith(SHOWDUINO_LEGACY_ACK_PREFIX) || c.startsWith("OK:") ||
      c.startsWith(SHOWDUINO_LEGACY_ERR_PREFIX)) {
    return true;
  }
  return false;
}

/* UART echo / Comms-only envelopes must never be executed or answered with
 * ERR:UNKNOWN_COMMAND. A reply storm freezes the Director LVGL desk. */
static bool isInboundTelemetryOrEcho(const String &c) {
  if (c.startsWith(SHOWDUINO_LEGACY_ROUTE_PREFIX)) return true;
  if (c.startsWith("STATE:") || c.startsWith("SNAPSHOT:") || c.startsWith("BOOT:") ||
      c.startsWith("FW:")) {
    return true;
  }
  if (c.startsWith(SHOWDUINO_LEGACY_ACK_PREFIX) || c.startsWith("OK:") ||
      c.startsWith(SHOWDUINO_LEGACY_ERR_PREFIX) || c.startsWith("REJECTED:") ||
      c.startsWith("UNSUPPORTED:")) {
    return true;
  }
  if (c == "READY" || c == SHOWDUINO_LEGACY_SHOWDUINO_STAGE) return true;
  if (c.startsWith("PIXELS:") || c.startsWith("ETHERNET:") || c.startsWith("INPUTS:") ||
      c.startsWith("SD:") || c.startsWith("ANNOUNCE:")) {
    return true;
  }
  if (c == "AUDIO:READY" || c == "AUDIO:FAULT") return true;
  if (c.startsWith(SHOWDUINO_LEGACY_STATUS_PREFIX) && c != SHOWDUINO_LEGACY_STATUS_REQUEST) {
    return true;
  }
  return false;
}

static bool commandLooksMalformed(const String &c) {
  if (c.length() == 0 || c.length() > SHOWDUINO_COMMS_CMD_MAX) return true;
  for (unsigned i = 0; i < c.length(); i++) {
    unsigned char ch = (unsigned char)c[i];
    if (ch < 32 || ch > 126) return true;
  }
  return false;
}

static void noteCoprocessorBootBanner() {
  sUartHushUntilMs = millis() + 2000UL;
  sUartNoiseCount++;
}

static void flushUartNoiseLog() {
  if (sUartNoiseCount == 0 || uartHushActive()) return;
  Serial.printf("[COMMS] ignored %u boot/noise line(s) — not Director commands\n",
                (unsigned)sUartNoiseCount);
  sUartNoiseCount = 0;
}

static void noteDirectorDeskSeen() {
  sDirectorPresent = true;
  sDirectorLastDeskMs = millis();
}

static void serviceDirectorPresence() {
  if (!sDirectorPresent) return;
  if ((millis() - sDirectorLastDeskMs) < SHOWDUINO_DIRECTOR_ABSENCE_MS) return;
  sDirectorPresent = false;
  Serial.println("[AUDIO] Director desk absent — next screen HELLO will play BOOT");
}

static void noteCommsValidRx(const String &command) {
  const uint32_t now = millis();
  sCommsLastRxMs = now;
  if (!command.startsWith("DIAG:") && !command.startsWith("NODE:")) sCommsSawDirector = true;
  if (!sCommsLinkUp) {
    sCommsLinkUp = true;
    SD_LOGI("COMMS", sCommsEverUp ? "Link restored" : "Link established");
    sCommsEverUp = true;
    SD_LOGT("COMMS", "RX: %s", command.c_str());
    return;
  }
  if (command == "HEARTBEAT" || command == "DIAG:PING" || command == "DIAG:PONG") {
    SD_LOGT("COMMS", "RX: %s", command.c_str());
    return;
  }
  if (command.startsWith("NODE:")) {
    /* AudioNodeLink / LampNodeLink print change-only INFO; raw packet is TRACE. */
    SD_LOGT("COMMS", "RX: %s", command.c_str());
    return;
  }
  SD_LOGT("COMMS", "RX: %s", command.c_str());
  if (!(command.startsWith("STATUS:") || command.startsWith("TIME:") ||
        command.startsWith("STATE:") || command.startsWith("SNAPSHOT:") ||
        command.startsWith("OK:") || command.startsWith("ACK:") ||
        command == "HELLO")) {
    SD_LOGD("COMMS", "RX cmd: %s", command.c_str());
  }
}

void serviceCommsLink() {
  if (!sCommsUartReady || !sCommsLinkUp) return;
  if ((millis() - sCommsLastRxMs) < SHOWDUINO_COMMS_LINK_TIMEOUT_MS) return;
  sCommsLinkUp = false;
  SD_LOGW("COMMS", "Link lost");
}

bool sendToDirector(const String &message) {
  if (!sCommsUartReady || uartHushActive()) {
    return false;
  }
  Serial1.println(message);
  const bool quiet =
      message == "OK:HEARTBEAT" ||
      message.startsWith(SHOWDUINO_LEGACY_TIME_PREFIX) ||
      message.startsWith("STATE:") ||
      message.startsWith("SNAPSHOT:") ||
      message.startsWith("FW:") ||
      message == "READY" ||
      message == SHOWDUINO_LEGACY_SHOWDUINO_STAGE;
  if (quiet) {
    SD_LOGT("COMMS", "TX: %s", message.c_str());
  } else {
    SD_LOGT("COMMS", "TX: %s", message.c_str());
    if (message.startsWith("ERR:") || message.startsWith("REJECTED:") ||
        message.startsWith("UNSUPPORTED:")) {
      SD_LOGW("P4", "%s", message.c_str());
    } else if (!(message.startsWith("ACK:") || message.startsWith("OK:") ||
                 message.startsWith("PIXELS:") || message.startsWith("AUDIO:") ||
                 message.startsWith("ETHERNET:") || message.startsWith("SD:") ||
                 message.startsWith("INPUTS:") || message.startsWith("DMX:") ||
                 message.startsWith("E131:"))) {
      SD_LOGD("COMMS", "TX event: %s", message.c_str());
    }
  }
  return true;
}

static void sendToDirectorC(const char *line) {
  if (line && line[0]) sendToDirector(String(line));
}

bool stageCommsUartReady() { return sCommsUartReady; }
bool stageCommsLinkUp() { return sCommsLinkUp; }
bool stageCommsEverUp() { return sCommsEverUp; }
uint32_t stageCommsLastRxMs() { return sCommsLastRxMs; }
bool stageCommsSawDirectorTraffic() { return sCommsSawDirector; }

void stageCommsSendLine(const char *line) {
  if (!sCommsUartReady || !line || !line[0]) return;
  Serial1.println(line);
  Serial1.flush();
}

void stageDiagDispatchLocal(const char *cmd) {
  if (!cmd) return;
  handleCommand(String(cmd), CommandSource::LocalUsb);
}

bool stageWebDispatchCommand(const char *cmd, String *repliesOut) {
  if (!cmd || !cmd[0]) return false;
  sWebReplySink = repliesOut;
  handleCommand(String(cmd), CommandSource::Comms);
  sWebReplySink = nullptr;
  return true;
}

bool stageEstopDebouncedAsserted() {
  return emergencyInputLoopOpen();
}

/* ACK / ERR / STATUS replies for the requester. State broadcasts still use sendToDirector. */
static void sendCommandReply(const String &message) {
  if (sWebReplySink) {
    if (sWebReplySink->length() > 0) *sWebReplySink += '\n';
    *sWebReplySink += message;
  }
  if (sCmdSource == CommandSource::LocalUsb) {
    Serial.print("[CONSOLE] ");
    Serial.println(message);
    return;
  }
  sendToDirector(message);
}

void triggerEmergency(EmergencySource source) {
  const bool already = emergencyLocked;
  const int gpioRaw =
#if SHOWDUINO_ESTOP_GPIO >= 0
      digitalRead(SHOWDUINO_ESTOP_GPIO);
#else
      -1;
#endif

  if (source == EmergencySource::Physical) {
    Serial.println("[ESTOP] Physical emergency triggered");
    Serial.printf("[ESTOP] TRIGGER source=PHYSICAL gpio=%d stable=%d latch=%s\n",
                  gpioRaw, emergencyInputStableOpen(), already ? "ACTIVE" : "CLEAR");
  } else if (source == EmergencySource::LocalUsb) {
    Serial.println("[ESTOP] Local USB emergency triggered");
    Serial.printf("[ESTOP] TRIGGER source=USB gpio=%d latch=%s cmd=EMERGENCY:STOP\n",
                  gpioRaw, already ? "ACTIVE" : "CLEAR");
  } else {
    Serial.println("[ESTOP] Remote emergency triggered");
    Serial.printf("[ESTOP] TRIGGER source=REMOTE gpio=%d latch=%s cmd=EMERGENCY:STOP\n",
                  gpioRaw, already ? "ACTIVE" : "CLEAR");
  }

  if (already) {
    Serial.println("[ESTOP] already latched — extra trigger ignored");
    stageLogEmergency("ACTIVATE_IGNORED", "already latched");
    return;
  }

  stageLogEmergency("ACTIVATE",
                    source == EmergencySource::Physical ? "source=physical" :
                    source == EmergencySource::LocalUsb ? "source=usb" : "source=remote");

  emergencyLocked = true;
  if (source == EmergencySource::Physical) {
    gEmergencySourceId = 2;
  } else if (source == EmergencySource::LocalUsb) {
    gEmergencySourceId = 3;
  } else {
    gEmergencySourceId = 1; /* director / comms */
  }
  gEngine.emergency = EmergencyState::Active;
  gEngine.show = ShowRuntimeState::Emergency;
  showEngineBump(gEngine);

  Serial.println("[ESTOP] EMERGENCY ACTIVE");
  showduino_log_emergency(true);
  Serial.printf("[ESTOP] source_id=%u emergency_pixels=%s show_pixels=%s audio_wav=%s\n",
                (unsigned)gEmergencySourceId,
                emergencyPixelsReady() ? "ready" : "not-ready",
                showPixelsReady() ? "ready" : "not-ready",
                stageAudioStatus().wavPresent ? "present" : "missing");

  stageAudioStopShow();
  audioNodeLinkOnEmergency(true);
  lampNodeLinkOnEmergency(true);
  gRuntime.onEmergencyStop(millis(), &gEngine);

  /* Hard Showduino rule: every pixel-capable local output goes bright white. */
  emergencyPixelsSetWhite();
  showPixelsOnEmergency(true);
  statusLedWrite(HIGH);
  pluginBusOnEmergency();

  sendToDirector(SHOWDUINO_LEGACY_STATUS_ELOCKED);
  sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_ACTIVE);
  sendToDirector(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + SHOWDUINO_WIRE_SHOW_EMERGENCY);
  Serial.println("[ESTOP] notified Director STATE:EMERGENCY:ACTIVE");

  if (!stageAudioStartEmergency()) {
    Serial.println("[ESTOP] Emergency audio failed — latch remains active");
    Serial.println("[ESTOP] WARNING: Emergency audio unavailable");
    Serial.println("[ESTOP] Emergency safety state remains operational");
  } else {
    Serial.println("[ESTOP] Emergency audio looping");
  }
}

static void rejectEmergencyClear(const char *code, const char *detail) {
  String line = String(SHOWDUINO_LEGACY_EMERGENCY_CLEAR_REJECTED_PREFIX) + code;
  sendCommandReply(line);
  if (strcmp(code, "BUTTON_ACTIVE") == 0) {
    sendCommandReply(SHOWDUINO_LEGACY_ERR_ESTOP_HELD);
  }
  Serial.printf("[ESTOP] Clear confirm rejected: %s\n", detail ? detail : code);
  stageLogEmergency("CLEAR_REJECT", code);
}

static void applyEmergencyClear() {
  Serial.println("[ESTOP] CLEAR authorised — releasing latch");
  emergencyInputCancelClear();
  stageAudioStopEmergency();
  emergencyLocked = false;
  gEmergencySourceId = 0;
  gEngine.emergency = EmergencyState::Clear;
  showEngineBump(gEngine);

  /* Signage returns to locator green. Show FX never auto-resume after emergency. */
  emergencyPixelsSetNormal();
  showPixelsOnEmergency(false);
  statusLedWrite(LOW);
  /* Latch wire first so Director unlocks before the runtime mirror arrives. */
  sendCommandReply(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  if (sCmdSource == CommandSource::LocalUsb) {
    sendToDirector(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  }
  sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
  audioNodeLinkOnEmergency(false);
  lampNodeLinkOnEmergency(false);
  gRuntime.onEmergencyCleared(millis(), &gEngine);
  Serial.println("[ESTOP] Emergency cleared");
  showduino_log_emergency(false);
  Serial.println("[SHOW] Returning to safe idle state");
  stageLogEmergency("CLEAR_SUCCESS", "latch released");
}

void clearEmergencyStop() {
  /* USB / diagnostics maintenance path. Still refuses a physically open loop. */
  if (physicalEstopAssertedNow()) {
    const int raw = emergencyInputRawGpio();
    Serial.printf("[ESTOP] Clear rejected: physical emergency button still asserted (gpio=%d stable=%d)\n",
                  raw, emergencyInputStableOpen());
    sendCommandReply(SHOWDUINO_LEGACY_ERR_ESTOP_HELD);
    if (!emergencyLocked) {
      triggerEmergency(EmergencySource::Physical);
    } else if (sCmdSource != CommandSource::LocalUsb) {
      sendToDirector(SHOWDUINO_LEGACY_STATUS_ELOCKED);
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_ACTIVE);
    }
    return;
  }

  if (!emergencyLocked) {
    Serial.println("[ESTOP] CLEAR ignored — latch already clear");
    sendCommandReply(SHOWDUINO_LEGACY_STATUS_ECLEARED);
    if (sCmdSource != CommandSource::LocalUsb) {
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
    }
    return;
  }

  applyEmergencyClear();
}

static void handleEmergencyClearConfirm() {
  const uint32_t now = millis();
  if (!emergencyLocked) {
    rejectEmergencyClear("NOT_LATCHED", "emergency not latched");
    return;
  }
  if (!emergencyInputPendingClear()) {
    rejectEmergencyClear("NO_REQUEST", "no pending clear request");
    return;
  }
  if (!emergencyInputPendingClearValid(now)) {
    emergencyInputCancelClear();
    rejectEmergencyClear("TIMEOUT", "clear request timed out");
    return;
  }
  if (physicalEstopAssertedNow()) {
    rejectEmergencyClear("BUTTON_ACTIVE", "button still active");
    if (sCmdSource != CommandSource::LocalUsb) {
      sendToDirector(SHOWDUINO_LEGACY_STATUS_ELOCKED);
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_ACTIVE);
    }
    return;
  }

  applyEmergencyClear();
  sendCommandReply(SHOWDUINO_LEGACY_EMERGENCY_CLEAR_OK);
}

static void handleEmergencyClearCancel() {
  emergencyInputCancelClear();
  Serial.println("[ESTOP] Clear request cancelled");
  sendCommandReply("EMERGENCY:CLEAR_CANCELLED");
}

void sendCapabilities() {
  sendCommandReply("SHOWDUINO_STAGE_ENGINE");
  sendCommandReply(String("FW:") + SHOWDUINO_P4_FIRMWARE_VERSION);
  sendCommandReply("DMX:PLANNED");
  sendCommandReply(showPixelsReady() ? "PIXELS:READY" : "PIXELS:FAULT");
  sendCommandReply(showNetworkLive().hasIp ? "ETHERNET:ONLINE" : "ETHERNET:OFFLINE");
  sendCommandReply(String("E131:") + e131RxStateName(e131ReceiverStatus().state));
  sendCommandReply(stageAudioStatus().codecReady ? "AUDIO:READY" : "AUDIO:FAULT");
  sendCommandReply("INPUTS:PLANNED");
  sendCommandReply(String("SD:") + stageStoreStateName());
  sendCommandReply(stageTimeSynced() ? "TIME:READY" : "TIME:UNSYNCED");
  sendCommandReply("READY");
}

static void printLocalConsoleStatus() {
  const StageAudioStatus &audio = stageAudioStatus();
  const char *prod = gRuntime.rt.showName[0] ? gRuntime.rt.showName : "(none)";
  Serial.printf("[CONSOLE] runtime=%s production=%s emergency=%s\n",
                showStateName(gRuntime.rt.state),
                prod,
                emergencyLocked ? "ACTIVE" : "CLEAR");
#if SHOWDUINO_ESTOP_GPIO >= 0
  Serial.printf("[CONSOLE] gpio25=%s stable=%s comms=%s director=%s\n",
                emergencyInputLoopOpen() ? "PRESSED" : "RELEASED",
                (emergencyInputStableOpen() > 0) ? "PRESSED" : "RELEASED",
                sCommsLinkUp ? "ALIVE" : "DOWN",
                sCommsSawDirector ? "ONLINE" : "not-seen");
#else
  Serial.printf("[CONSOLE] gpio25=unassigned comms=%s director=%s\n",
                sCommsLinkUp ? "ALIVE" : "DOWN",
                sCommsSawDirector ? "ONLINE" : "not-seen");
#endif
  char iso[24];
  stageTimeIso(iso, sizeof(iso));
  Serial.printf("[CONSOLE] rtc=%s source=%s %s\n",
                stageTimeHealth(), stageTimeSource(), iso);
  Serial.printf("[CONSOLE] sd=%s sysaudio=%s current=%s emergency_wav=%s plugins=%u\n",
                stageStoreStateName(),
                audio.healthName,
                audio.currentName,
                audio.wavPresent ? "present" : "missing",
                (unsigned)pluginBusInstanceCount());
  Serial.printf("[CONSOLE] emergencyPixels=%s showPixels=%s showPixelCount=%u showBrightness=%u\n",
                emergencyPixelsReady() ? "READY" : "FAULT",
                showPixelsReady() ? "READY" : "FAULT",
                (unsigned)showPixelsCount(),
                (unsigned)showPixelsGlobalBrightness());
}

void sendStatus() {
  sendCommandReply(SHOWDUINO_WIRE_SNAPSHOT_BEGIN);
  sendCommandReply(emergencyLocked ? SHOWDUINO_LEGACY_STATUS_ELOCKED : "STATUS:READY");
  sendCommandReply(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) +
                   (emergencyLocked ? SHOWDUINO_WIRE_EMERGENCY_ACTIVE : SHOWDUINO_WIRE_EMERGENCY_CLEAR));
  sendCommandReply(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + showRuntimeWire(gEngine.show));
  sendCommandReply(SHOWDUINO_WIRE_SNAPSHOT_END);
  audioNodeLinkPublishToDirector();
  lampNodeLinkPublishToDirector();
  {
    char timeWire[96];
    if (stageTimeFormatDirectorWire(timeWire, sizeof(timeWire))) {
      sendCommandReply(timeWire);
    }
  }
  gRuntime.handleStateQuery();
  if (sCmdSource == CommandSource::LocalUsb) {
    printLocalConsoleStatus();
  }
}

static void sendProductionError(const char *operation, ProductionStoreResult result) {
  String response = "PRODUCTION:";
  response += operation;
  response += ":ERROR:";
  response += productionStoreResultName(result);
  sendCommandReply(response);
}

static void handleProductionCommand(const String &command) {
  const uint32_t now = millis();

  if (command == "PRODUCTION:LIST") {
    if (!sProductionStoreReady) {
      sendProductionError("LIST", ProductionStoreResult::StorageUnavailable);
      return;
    }
    if (!gProductionStore.scan()) {
      sendProductionError("LIST", ProductionStoreResult::IoError);
      return;
    }
    sendCommandReply(String("PRODUCTION:LIST:BEGIN:") + gProductionStore.count());
    for (uint8_t i = 0; i < gProductionStore.count(); ++i) {
      const ProductionManifest *manifest = gProductionStore.at(i);
      if (!manifest) continue;
      sendCommandReply(String("PRODUCTION:ITEM:") + manifest->productionId);
    }
    sendCommandReply("PRODUCTION:LIST:END");
    return;
  }

  if (command == "PRODUCTION:STATUS") {
    if (!sProductionStoreReady) {
      sendCommandReply("PRODUCTION:STATUS:STORAGE_UNAVAILABLE");
    } else if (!gProductionStore.hasLoaded()) {
      sendCommandReply("PRODUCTION:STATUS:NONE");
    } else {
      sendCommandReply(String("PRODUCTION:STATUS:LOADED:") +
                       gProductionStore.loaded().productionId);
    }
    return;
  }

  if (command == "PRODUCTION:UNLOAD") {
    if (emergencyLocked) {
      sendCommandReply("PRODUCTION:UNLOAD:ERROR:EMERGENCY_ACTIVE");
      return;
    }
    if (gRuntime.handleUnload(now, &gEngine)) {
      gProductionStore.unload();
      showPixelsBlackout();
      Serial.println("[PRODUCTION] Unloaded");
      sendCommandReply("PRODUCTION:UNLOAD:OK");
    }
    return;
  }

  if (command.startsWith("PRODUCTION:LOAD:")) {
    if (emergencyLocked) {
      sendCommandReply("PRODUCTION:LOAD:ERROR:EMERGENCY_ACTIVE");
      return;
    }
    if (!sProductionStoreReady) {
      sendProductionError("LOAD", ProductionStoreResult::StorageUnavailable);
      return;
    }
    if (gRuntime.rt.state == SHOW_STATE_RUNNING ||
        gRuntime.rt.state == SHOW_STATE_PAUSED ||
        gRuntime.rt.state == SHOW_STATE_EMERGENCY_STOP) {
      sendCommandReply("PRODUCTION:LOAD:ERROR:BUSY");
      return;
    }

    String id = command.substring(strlen("PRODUCTION:LOAD:"));
    id.trim();
    ProductionPackage package{};
    ProductionStoreResult result = gProductionStore.load(id.c_str(), &package);
    if (result != ProductionStoreResult::Ok) {
      Serial.printf("[PRODUCTION] ERROR: %s (%s)\n",
                    productionStoreResultName(result), gProductionStore.lastError());
      sendProductionError("LOAD", result);
      gProductionStore.release(&package);
      return;
    }

    /* The TimelineEngine stages a replacement buffer. The active production is
       left untouched unless every validated cue reaches the new buffer. */
    if (!gRuntime.handleTlBegin(false)) {
      gProductionStore.release(&package);
      sendProductionError("LOAD", ProductionStoreResult::NoMemory);
      return;
    }
    bool staged = true;
    for (uint16_t i = 0; i < package.timeline.cueCount; ++i) {
      if (!gRuntime.handleTlCue(package.cues[i].timeMs, package.cues[i].command)) {
        staged = false;
        break;
      }
    }
    if (!staged || !gRuntime.handleTlEnd(now, &gEngine, package.manifest.name, false)) {
      gRuntime.abortTimelineLoad();
      gProductionStore.release(&package);
      Serial.println("[PRODUCTION] ERROR: timeline commit failed");
      sendProductionError("LOAD", ProductionStoreResult::NoMemory);
      return;
    }

    gProductionStore.markLoaded(package.manifest);
    String loadedId = package.manifest.productionId;
    uint16_t cueCount = package.timeline.cueCount;
    gProductionStore.release(&package);
    stageConfigSetLastProduction(loadedId.c_str());
    stageLogWrite(StageLogChannel::Production, "INFO", "production loaded");
    Serial.printf("[TIMELINE] %u cues loaded\n", (unsigned)cueCount);
    Serial.println("[PRODUCTION] Load complete");
    sendCommandReply(String("PRODUCTION:LOAD:OK:") + loadedId);
    return;
  }

  sendCommandReply("PRODUCTION:ERROR:UNKNOWN_COMMAND");
}

void handleShowCommand(const String &command) {
  const uint32_t now = millis();

  if (command == "SHOW:STATE?") {
    if (sCmdSource == CommandSource::Comms) {
      noteDirectorDeskSeen();
    }
    gRuntime.handleStateQuery();
    return;
  }

  if (command == "SHOW:START" || command == "SHOW:RUN") {
    if (emergencyLocked) {
      Serial.println("[SHOW] Start rejected: EMERGENCY ACTIVE");
      sendCommandReply("SHOW:START:REJECTED:EMERGENCY");
      return;
    }
    if (gRuntime.timeline.cueTotal() == 0) {
      sendCommandReply("SHOW:START:REJECTED:NO_PRODUCTION");
      return;
    }
    if (gRuntime.handleRun(now, &gEngine)) {
      SD_LOGI("P4", "Show START");
      sendCommandReply("SHOW:START:OK");
    }
    return;
  }

  if (command.startsWith("SHOW:RUN:") || command.startsWith("SHOW:LOAD:")) {
    int colon = command.indexOf(':', 5);
    String name = (colon >= 0) ? command.substring(colon + 1) : "";
    name.trim();
    if (!gRuntime.handleLoadName(name.c_str(), now, &gEngine)) return;
    gProductionStore.unload();
    showPixelsBlackout();
    return;
  }

  if (command == "SHOW:PAUSE") {
    if (emergencyLocked) {
      sendCommandReply("SHOW:PAUSE:REJECTED:EMERGENCY");
      return;
    }
    if (gRuntime.handlePause(now, &gEngine)) {
      SD_LOGI("P4", "Show PAUSE");
      sendCommandReply("SHOW:PAUSE:OK");
    }
    return;
  }

  if (command == "SHOW:RESUME") {
    if (emergencyLocked) {
      sendCommandReply("SHOW:RESUME:REJECTED:EMERGENCY");
      return;
    }
    if (gRuntime.handleResume(now, &gEngine)) {
      SD_LOGI("P4", "Show RESUME");
      sendCommandReply("SHOW:RESUME:OK");
    }
    return;
  }

  if (command == "SHOW:STOP" || command == "STOP:ALL") {
    bool stopped = gRuntime.handleStop(now, &gEngine);
    if (!emergencyLocked) {
      stageAudioStopShow();
      showPixelsBlackout();
    }
    if (stopped) {
      SD_LOGI("P4", "Show STOP");
      sendCommandReply("SHOW:STOP:OK");
    }
    return;
  }

  if (command == "SHOW:TL:BEGIN") {
    if (emergencyLocked) {
      sendCommandReply("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    gRuntime.handleTlBegin();
    return;
  }

  if (command.startsWith("SHOW:TL:C:")) {
    if (emergencyLocked) {
      sendCommandReply("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    String rest = command.substring(strlen("SHOW:TL:C:"));
    int colon = rest.indexOf(':');
    uint32_t t = (colon >= 0) ? (uint32_t)rest.substring(0, colon).toInt() : rest.toInt();
    String cmd = (colon >= 0) ? rest.substring(colon + 1) : "";
    if (!gRuntime.handleTlCue(t, cmd.c_str())) {
      sendCommandReply("ERR:SHOW:TL:C");
    }
    return;
  }

  if (command == "SHOW:TL:END") {
    if (emergencyLocked) {
      sendCommandReply("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    if (gRuntime.handleTlEnd(now, &gEngine)) gProductionStore.unload();
    return;
  }

  sendCommandReply("OK:SHOW:COMMAND_RECEIVED");
}

void handleAudioCommand(const String &command) {
  if (command.startsWith("AUDIO:NODE:")) {
    char reply[80];
    audioNodeLinkHandleCommand(command.c_str(), reply, sizeof(reply));
    if (reply[0]) sendCommandReply(reply);
    return;
  }

  char reply[96];
  if (stageAudioHandleCommand(command.c_str(), reply, sizeof(reply))) {
    if (reply[0]) sendCommandReply(reply);
    return;
  }

  sendCommandReply("UNSUPPORTED:AUDIO");
}

static void printUsbHelp() {
  Serial.println("[CONSOLE] Showduino P4 commands:");
  Serial.println("  STATUS:REQUEST");
  Serial.println("  TIME:REQUEST");
  Serial.println("  TIME:SET:<epoch> | TIME:SET:YYYY-MM-DDTHH:MM:SSZ");
  Serial.println("  SHOW:START");
  Serial.println("  SHOW:STOP");
  Serial.println("  SHOW:PAUSE");
  Serial.println("  SHOW:RESUME");
  Serial.println("  SHOW:LOAD:<name>");
  Serial.println("  PRODUCTION:LIST");
  Serial.println("  PRODUCTION:LOAD:<id>");
  Serial.println("  PRODUCTION:UNLOAD");
  Serial.println("  PRODUCTION:STATUS");
  Serial.println("  EMERGENCY:STOP");
  Serial.println("  EMERGENCY:CLEAR            (USB maintenance; loop must be healthy)");
  Serial.println("  EMERGENCY:CLEAR_CONFIRM    (dual-action; requires pending request)");
  Serial.println("  EMERGENCY:CLEAR_CANCEL");
  Serial.println("  PIXEL:STATUS");
  Serial.println("  PIXEL:TEST | PIXEL:TEST:STOP");
  Serial.println("  PIXEL:OFF | PIXEL:BLACKOUT");
  Serial.println("  PIXEL:SOLID:<r>:<g>:<b>");
  Serial.println("  PIXEL:BRIGHTNESS:<0-255>");
  Serial.println("  PIXEL:SEGMENT:<id>:RANGE:<start>:<count>");
  Serial.println("  PIXEL:SEGMENT:<id>:FX:<name>");
  Serial.println("  PIXEL:SEGMENT:<id>:COLOR:<r>:<g>:<b>");
  Serial.println("  PIXEL:SEGMENT:<id>:COLOR2:<r>:<g>:<b>");
  Serial.println("  PIXEL:SEGMENT:<id>:BRIGHTNESS|SPEED|INTENSITY|RANDOMNESS:<value>");
  Serial.println("  PIXEL:SEGMENT:<id>:DURATION:<ms>");
  Serial.println("  PIXEL:SEGMENT:<id>:REVERSE:<0|1>");
  Serial.println("  PIXEL:SEGMENT:<id>:START | STOP | STATUS");
  Serial.println("  PLUGIN:SCAN");
  Serial.println("  PLUGIN:LIST");
  Serial.println("  PLUGIN:STATUS");
  Serial.println("  PLUGIN:INFO:<instance|address>");
  Serial.println("  NET:STATUS");
  Serial.println("  NET:ENABLE:0|1");
  Serial.println("  NET:MODE:DHCP");
  Serial.println("  NET:STATIC:<ip>:<mask>:<gw>[:dns]");
  Serial.println("  E131:STATUS");
  Serial.println("  E131:CHANNELS");
  Serial.println("  E131:CHANNELS:<from>:<count>");
  Serial.println("  E131:ENABLE:0|1");
  Serial.println("  E131:UNIVERSE:<n>");
  Serial.println("  STORAGE:STATUS");
  Serial.println("  STORAGE:LIST");
  Serial.println("  STORAGE:CHECK");
  Serial.println("  STORAGE:BACKUP");
  Serial.println("  AUDIO:STATUS");
  Serial.println("  AUDIO:TEST:BOOT | EMERGENCY | BEEP | TONE");
  Serial.println("  AUDIO:TEST:ERROR | ACCEPTED | COMPLETE");
  Serial.println("  AUDIO:STOP");
  Serial.println("  AUDIO:NODE:PLAY:<path>");
  Serial.println("  AUDIO:NODE:LOOP:<path>");
  Serial.println("  AUDIO:NODE:STOP | PAUSE | RESUME");
  Serial.println("  AUDIO:NODE:VOLUME:<0-100>");
  Serial.println("  AUDIO:NODE:SOUND:STATUS|ENABLE|DISABLE|CALIBRATE|TRIGGER:TEST");
  Serial.println("  AUDIO:NODE:SOUND:THRESHOLD:<0-100>");
  Serial.println("  RUN:TEST");
  Serial.println("  RUN:TEST:STATUS");
  Serial.println("  RUN:TEST:ABORT");
  Serial.println("  CONFIRM:PIXELS:YES | CONFIRM:PIXELS:NO");
  Serial.println("  CONFIRM:AUDIO:YES | CONFIRM:AUDIO:NO");
  Serial.println("  LOG:LEVEL | LOG:LEVEL:ERROR|WARN|INFO|DEBUG|TRACE");
  Serial.println("  HELP");
}

static void emitConsoleLine(const char *line) {
  if (line && line[0]) {
    Serial.print("[CONSOLE] ");
    Serial.println(line);
  }
}

static void emitRuntimeToUsbAndDirector(const char *line) {
  emitConsoleLine(line);
  sendToDirectorC(line);
}

static void dispatchCommand(const String &command) {
  if (command.startsWith("RUN:TEST") || command.startsWith("CONFIRM:")) {
    if (sCmdSource != CommandSource::LocalUsb) return;
    (void)stageDiagHandleCommand(command.c_str());
    return;
  }

  if (sCmdSource == CommandSource::LocalUsb && command == "HELP") {
    printUsbHelp();
    return;
  }

  if (sCmdSource == CommandSource::LocalUsb &&
      showduino_log_handle_command(command.c_str())) {
    return;
  }

  if (command.startsWith("WEB/")) {
    webApiHandleTunnelRequest(command);
    return;
  }

  if (isInboundTelemetryOrEcho(command)) {
    return;
  }

  if (command == "HELLO") {
    sendCapabilities();
    audioNodeLinkPublishToDirector();
    lampNodeLinkPublishToDirector();
    if (sCmdSource == CommandSource::Comms) {
      const bool welcome = !sDirectorPresent;
      noteDirectorDeskSeen();
      if (welcome) {
        stageAudioOnDirectorHello();
      }
    }
    return;
  }

  if (command == "HEARTBEAT") {
    if (sCmdSource != CommandSource::LocalUsb) {
      lastHeartbeatMs = millis();
      noteDirectorDeskSeen();
    }
    sendCommandReply("OK:HEARTBEAT");
    return;
  }

  if (command == "STATUS:REQUEST") {
    if (sCmdSource == CommandSource::Comms) {
      noteDirectorDeskSeen();
    }
    sendStatus();
    return;
  }

  if (command == SHOWDUINO_LEGACY_TIME_REQUEST || command == "TIME?") {
    if (sCmdSource == CommandSource::Comms) {
      noteDirectorDeskSeen();
    }
    char timeWire[96];
    if (stageTimeFormatDirectorWire(timeWire, sizeof(timeWire))) {
      sendCommandReply(timeWire);
    } else {
      sendCommandReply("TIME:0|--:--:--|---|offline|none");
    }
    return;
  }

  if (command.startsWith("TIME:SET:")) {
    uint32_t epoch = 0;
    if (!stageTimeParseSet(command.c_str() + 9, &epoch) || !stageTimeSetEpoch(epoch)) {
      sendCommandReply("ERR:TIME:SET");
      return;
    }
    char timeWire[96];
    if (stageTimeFormatDirectorWire(timeWire, sizeof(timeWire))) {
      sendCommandReply(timeWire);
      if (sCmdSource == CommandSource::LocalUsb) sendToDirector(timeWire);
    }
    sendCommandReply("ACK:TIME:SET");
    return;
  }

  if (command.startsWith(SHOWDUINO_LEGACY_TIME_PREFIX)) {
    return;
  }

  if (command == "PANIC" || command == "EMERGENCY:PANIC" ||
      command == "ESTOP" || command == "E-STOP") {
    Serial.printf("[ESTOP] %s cmd %s → EMERGENCY:STOP\n",
                  sCmdSource == CommandSource::LocalUsb ? "USB" : "UART",
                  command.c_str());
    triggerEmergency(sCmdSource == CommandSource::LocalUsb
                         ? EmergencySource::LocalUsb
                         : EmergencySource::Remote);
    return;
  }

  if (command == "EMERGENCY:STOP") {
    Serial.printf("[ESTOP] %s cmd EMERGENCY:STOP\n",
                  sCmdSource == CommandSource::LocalUsb ? "USB" : "UART");
    triggerEmergency(sCmdSource == CommandSource::LocalUsb
                         ? EmergencySource::LocalUsb
                         : EmergencySource::Remote);
    return;
  }

  if (command == "EMERGENCY:CLEAR") {
    Serial.printf("[ESTOP] %s cmd EMERGENCY:CLEAR\n",
                  sCmdSource == CommandSource::LocalUsb ? "USB" : "UART");
    if (sCmdSource == CommandSource::LocalUsb) {
      clearEmergencyStop();
    } else {
      handleEmergencyClearConfirm();
    }
    return;
  }

  if (command == SHOWDUINO_LEGACY_EMERGENCY_CLEAR_CONFIRM) {
    Serial.printf("[ESTOP] %s cmd EMERGENCY:CLEAR_CONFIRM\n",
                  sCmdSource == CommandSource::LocalUsb ? "USB" : "UART");
    handleEmergencyClearConfirm();
    return;
  }

  if (command == SHOWDUINO_LEGACY_EMERGENCY_CLEAR_CANCEL) {
    Serial.printf("[ESTOP] %s cmd EMERGENCY:CLEAR_CANCEL\n",
                  sCmdSource == CommandSource::LocalUsb ? "USB" : "UART");
    handleEmergencyClearCancel();
    return;
  }

  if (command.startsWith("PRODUCTION:")) {
    handleProductionCommand(command);
    return;
  }

  if (command.startsWith("SHOW:") || command == "STOP:ALL") {
    handleShowCommand(command);
    return;
  }

  if (command.startsWith("NODE:AUDIO:")) {
    audioNodeLinkHandleReport(command.c_str());
    return;
  }

  if (command.startsWith("NODE:LAMP:")) {
    lampNodeLinkHandleReport(command.c_str());
    return;
  }

  if (command.startsWith(SHOWDUINO_LEGACY_NODE_PREFIX)) {
    static uint32_t sLastUnhandledNodeMs = 0;
    if ((int32_t)(millis() - sLastUnhandledNodeMs) >= 5000) {
      sLastUnhandledNodeMs = millis();
      Serial.printf("[COMMS] node report ignored (no handler yet): %s\n", command.c_str());
    }
    return;
  }

  if (command.startsWith("AUDIO:")) {
    handleAudioCommand(command);
    return;
  }

  if (command.startsWith("LAMP:")) {
    char reply[80];
    lampNodeLinkHandleCommand(command.c_str(), reply, sizeof(reply));
    if (reply[0]) sendCommandReply(reply);
    return;
  }

  if (command.startsWith("DMX:")) {
    sendCommandReply("UNSUPPORTED:DMX");
    return;
  }

  if (command.startsWith("PIXEL:")) {
    char reply[180];
    if (showPixelsHandleCommand(command.c_str(), reply, sizeof(reply))) {
      if (reply[0]) sendCommandReply(reply);
    } else {
      sendCommandReply("PIXEL:ERROR:NOT_HANDLED");
    }
    return;
  }

  if (command.startsWith("NET:")) {
    if (showNetworkHandleCommand(command)) {
      sendCommandReply(command == "NET:STATUS" ? "ACK:NET:STATUS" : "ACK:NET");
    } else {
      sendCommandReply("ERR:NET");
    }
    return;
  }

  if (command.startsWith("E131:")) {
    if (e131ReceiverHandleCommand(command)) {
      sendCommandReply(command == "E131:STATUS" ? "ACK:E131:STATUS" : "ACK:E131");
    } else {
      sendCommandReply("ERR:E131");
    }
    return;
  }

  if (command.startsWith("STORAGE:")) {
    if (stageStoreHandleCommand(command)) {
      sendCommandReply(command == "STORAGE:STATUS" ? "ACK:STORAGE:STATUS" : "ACK:STORAGE");
    } else {
      sendCommandReply("ERR:STORAGE");
    }
    return;
  }

  if (command == "PLUGIN:SCAN") {
    pluginBusScan();
    sendCommandReply(String("PLUGIN:SCAN:OK:") + pluginBusInstanceCount());
    return;
  }

  if (command == "PLUGIN:LIST") {
    pluginBusPrintList();
    return;
  }

  if (command == "PLUGIN:STATUS") {
    pluginBusPrintStatus();
    return;
  }

  if (command.startsWith("PLUGIN:INFO:")) {
    String key = command.substring(strlen("PLUGIN:INFO:"));
    key.trim();
    pluginBusPrintInfo(key.c_str());
    return;
  }

  if (command == "PLUGIN:INFO") {
    pluginBusPrintInfo("");
    return;
  }

  Serial.printf("[CMD] UNKNOWN command=%s\n", command.c_str());
  String reply = "ERR:UNKNOWN_COMMAND:";
  reply += command;
  if (reply.length() > 80) reply = reply.substring(0, 80);
  sendCommandReply(reply);
}

void handleCommand(String command, CommandSource source) {
  command.trim();
  if (command.length() == 0) return;

  const CommandSource prevSource = sCmdSource;
  const ShowRuntimeSendFn prevSend = gRuntime.sendFn;
  sCmdSource = source;

  if (source == CommandSource::LocalUsb) {
    if (commandLooksMalformed(command)) {
      Serial.println("[CONSOLE] ERROR: malformed command");
      sCmdSource = prevSource;
      return;
    }
    const bool localOnly = (command == "HELP" || command == "STATUS:REQUEST" ||
                            command == "HELLO" || command == "HEARTBEAT" ||
                            command.startsWith("LOG:LEVEL") ||
                            command.startsWith("TIME:") ||
                            command.startsWith("PIXEL:") ||
                            command.startsWith("PLUGIN:") ||
                            command.startsWith("NET:") ||
                            command.startsWith("E131:") ||
                            command.startsWith("RUN:TEST") ||
                            command.startsWith("CONFIRM:"));
    gRuntime.sendFn = localOnly ? emitConsoleLine : emitRuntimeToUsbAndDirector;
    if (command != "HEARTBEAT" && !command.startsWith("LOG:LEVEL")) {
      Serial.printf("[CMD] source=%s command=%s\n",
                    source == CommandSource::LocalUsb ? "USB" : "COMMS",
                    command.c_str());
    }
  } else {
    if (isCoprocessorBootBanner(command) || commandLooksMalformed(command)) {
      if (isCoprocessorBootBanner(command)) {
        noteCoprocessorBootBanner();
      } else {
        Serial.println("[COMMS] Malformed packet rejected");
      }
      sCmdSource = prevSource;
      return;
    }
    if (uartHushActive() && !isKnownCommsCommand(command)) {
      sUartNoiseCount++;
      sCmdSource = prevSource;
      return;
    }
    if (command == "DIAG:PING") {
      sCommsLastRxMs = millis();
      if (!sCommsLinkUp) {
        sCommsLinkUp = true;
        Serial.println(sCommsEverUp ? "[COMMS] Link restored" : "[COMMS] Link established");
        sCommsEverUp = true;
      }
      Serial.println("[COMMS] UART RX: DIAG:PING");
      Serial1.println("DIAG:PONG");
      Serial1.flush();
      Serial.println("[COMMS] UART TX: DIAG:PONG");
      sCmdSource = prevSource;
      gRuntime.sendFn = prevSend;
      return;
    }
    if (command == "DIAG:PONG") {
      sCommsLastRxMs = millis();
      if (!sCommsLinkUp) {
        sCommsLinkUp = true;
        Serial.println(sCommsEverUp ? "[COMMS] Link restored" : "[COMMS] Link established");
        sCommsEverUp = true;
      }
      stageDiagNoteCommsLine(command.c_str());
    } else {
      stageDiagNoteCommsLine(command.c_str());
      noteCommsValidRx(command);
    }
  }

  if (command == "DIAG:PONG") {
    stageDiagNoteCommsPong();
    sCmdSource = prevSource;
    gRuntime.sendFn = prevSend;
    return;
  }

  dispatchCommand(command);

  sCmdSource = prevSource;
  gRuntime.sendFn = prevSend;
}

void handleCommand(String command) {
  handleCommand(command, CommandSource::Comms);
}

static void timelineDispatchCommand(const char *command) {
  if (!command || !command[0]) return;
  static const char *const kInternalPrefix = "INTERNAL:";
  if (strncmp(command, kInternalPrefix, strlen(kInternalPrefix)) == 0) {
    const char *type = command + strlen(kInternalPrefix);
    const char *typeSeparator = strchr(type, ':');
    if (!typeSeparator) return;
    char cueType[8];
    size_t typeLen = (size_t)(typeSeparator - type);
    if (typeLen >= sizeof(cueType)) typeLen = sizeof(cueType) - 1;
    memcpy(cueType, type, typeLen);
    cueType[typeLen] = '\0';
    const char *id = typeSeparator + 1;
    const char *separator = strchr(id, ':');
    Serial.printf("[SHOW] Internal %s cue\n", cueType);
    if (separator) {
      char cueId[SHOWDUINO_CUE_ID_MAX];
      size_t idLen = (size_t)(separator - id);
      if (idLen >= sizeof(cueId)) idLen = sizeof(cueId) - 1;
      memcpy(cueId, id, idLen);
      cueId[idLen] = '\0';
      Serial.printf("[SHOW] Cue fired: %s\n", cueId);
      Serial.printf("[SHOW] Type: %s\n", cueType);
      Serial.printf("[SHOW] Time: %lu ms\n",
                    (unsigned long)gRuntime.timeline.CurrentTime());
      Serial.printf("[SHOW] Message: %s\n", separator + 1);
    } else {
      Serial.printf("[SHOW] Message: %s\n", id);
    }
    return;
  }
  handleCommand(String(command));
}

void readCommsSerial() {
  if (!sCommsUartReady) return;
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        handleCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;

      if (inputBuffer.length() > SHOWDUINO_COMMS_CMD_MAX) {
        inputBuffer = "";
        Serial.println("[COMMS] Malformed packet rejected");
      }
    }
  }
}

void readUsbSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (sUsbOverflow) {
        sUsbOverflow = false;
        sUsbLen = 0;
        Serial.println("[CONSOLE] ERROR: command too long");
        continue;
      }
      if (sUsbLen == 0) continue;
      sUsbLine[sUsbLen] = '\0';
      /* USB CDC often injects leading/embedded non-printables on connect.
       * Drop those lines here. Parser test 13 still uses handleCommand() directly. */
      uint16_t start = 0;
      while (start < sUsbLen && (unsigned char)sUsbLine[start] < 32) start++;
      uint16_t out = 0;
      bool binary = false;
      for (uint16_t i = start; i < sUsbLen; i++) {
        unsigned char ch = (unsigned char)sUsbLine[i];
        if (ch < 32 || ch > 126) {
          binary = true;
          break;
        }
        sUsbLine[out++] = sUsbLine[i];
      }
      if (binary || out == 0) {
        sUsbLen = 0;
        continue;
      }
      sUsbLine[out] = '\0';
      handleCommand(String(sUsbLine), CommandSource::LocalUsb);
      sUsbLen = 0;
    } else {
      if (sUsbOverflow) continue;
      if (sUsbLen >= SHOWDUINO_COMMS_CMD_MAX) {
        sUsbOverflow = true;
        sUsbLen = 0;
        continue;
      }
      sUsbLine[sUsbLen++] = c;
    }
  }
}

void servicePhysicalEstop() {
  /* Momentary pushbutton: first debounced press still latches immediately.
   * Release does not clear. Extra gestures never weaken that latch. */
  const EmergencyInputEvents ev = emergencyInputService(millis());
  if (ev.loopOpened) {
    if (!emergencyLocked) {
      triggerEmergency(EmergencySource::Physical);
      Serial.println("[ESTOP] Emergency latched");
    } else {
      Serial.println("[ESTOP] press ignored — already latched");
    }
  }
  if (ev.locateRequested) {
    Serial.println("[ESTOP] DIRECTOR:LOCATE");
    sendToDirector(SHOWDUINO_LEGACY_DIRECTOR_LOCATE);
  }
  if (ev.clearRequested) {
    Serial.println("[ESTOP] Hold detected");
    Serial.println("[ESTOP] Clear request sent");
    stageLogEmergency("CLEAR_REQUEST", "physical hold");
    sendToDirector(SHOWDUINO_LEGACY_EMERGENCY_CLEAR_REQUEST);
  }
  if (ev.clearExpired) {
    Serial.println("[ESTOP] Clear request expired");
    stageLogEmergency("CLEAR_EXPIRED", "request timeout");
    sendToDirector(SHOWDUINO_LEGACY_EMERGENCY_CLEAR_EXPIRED);
  }
}

static void pumpLocalServices() {
  servicePhysicalEstop();
  readCommsSerial();
  readUsbSerial();
}

void setup() {
  unsigned long bootMs = millis();
  Serial.begin(DEBUG_BAUD);
  delay(200);

  Serial.println();
  Serial.println("[BOOT] Showduino P4 starting");

  stageTimeBegin();

#if SHOWDUINO_STATUS_LED_PIN >= 0
  pinMode(SHOWDUINO_STATUS_LED_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_STATUS_LED_PIN, LOW);
#else
  Serial.println("[BOOT] No status LED GPIO — GPIO10 is ES8311 LRCK, left unused");
#endif

  /* Button before pixels. A failed pixel driver must never prevent GPIO25 setup. */
#if SHOWDUINO_ESTOP_GPIO >= 0
  emergencyInputBegin();
#else
  Serial.println("[ESTOP] Emergency input initialized (command path only; GPIO not assigned)");
#endif

  emergencyPixelsBegin();
  showPixelsBegin();

  stageStorageSetLinkPump(pumpLocalServices);
  Serial.println("[SD] Mounting SD card...");
  bool sdOk = stageStorageBegin();
  if (sdOk) {
    Serial.println("[SD] SD mounted");
    Serial.println("[SD] SD initialization success");
    sProductionStoreReady = gProductionStore.begin(stageStorageFs());
    sProductionStoreRetryMs = millis();
  } else {
    Serial.println("[SD] ERROR: SD card unavailable");
    Serial.println("[WEB] WebUI unavailable - SD not mounted");
  }

  gRuntime.begin(sendToDirectorC);
  gRuntime.setDispatch(timelineDispatchCommand);

  stageAudioBegin();
  if (!sdOk) {
    Serial.println("[ESTOP] WARNING: Emergency audio unavailable (no SD)");
    Serial.println("[ESTOP] Emergency safety state remains operational");
  }

  Serial.println("[COMMS] Initialising communications link");
  Serial1.setRxBufferSize(SHOWDUINO_COMMS_UART_RX_BUFFER);
  Serial1.begin(SHOWDUINO_COMMS_UART_BAUD, SERIAL_8N1,
                SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN);
  sCommsUartReady = true;
  Serial.printf("[COMMS] UART RX=%d TX=%d baud=%u 8N1 newline-framed\n",
                SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN,
                (unsigned)SHOWDUINO_COMMS_UART_BAUD);
  Serial.println("[COMMS] Waiting for communications controller (continuing locally)");
  readCommsSerial();

  pluginBusBegin(pumpLocalServices);
  if (!stageAudioInitHardware()) {
    Serial.println("[AUDIO] ES8311/I2S not ready — system WAV disabled");
    Serial.println("[AUDIO] Emergency latch and Audio Node routing still work");
  } else if (!stageAudioStatus().wavPresent) {
    Serial.println("[ESTOP] WARNING: emergency.wav missing — latch still works");
  }
  Serial.println("[AUDIO] BOOT waits for Director HELLO (screen power-on)");
  audioNodeLinkBegin();
  lampNodeLinkBegin();

  stageStoreBegin();
  webApiBegin(bootMs);
  showNetworkBegin();

  gRuntime.bootToIdle();
  lastHeartbeatMs = millis();
  Serial.println("[SYSTEM] Showduino ready");
  sendToDirector("BOOT:STAGE_ENGINE_READY");
  readCommsSerial();
  Serial.println("[CONSOLE] USB command console ready — type HELP");
}

void loop() {
  readCommsSerial();
  readUsbSerial();
  flushUartNoiseLog();
  serviceCommsLink();
  serviceDirectorPresence();
  servicePhysicalEstop();
  pluginBusService();
  stageStorageLoop();
  stageStoreLoop();
  if (!sProductionStoreReady && stageStorageIsReady() &&
      (millis() - sProductionStoreRetryMs) >= 15000UL) {
    sProductionStoreRetryMs = millis();
    sProductionStoreReady = gProductionStore.begin(stageStorageFs());
  }
  stageAudioLoop();
  audioNodeLinkLoop();
  lampNodeLinkLoop();
  stageTimeLoop(millis(), sendToDirectorC);
  showNetworkLoop();
  gRuntime.service(millis(), &gEngine);
  showPixelsService();

  if (emergencyLocked) {
    statusLedWrite((millis() / 250) % 2 == 0 ? HIGH : LOW);
    emergencyPixelsService();
  }
  stageDiagService();
}
