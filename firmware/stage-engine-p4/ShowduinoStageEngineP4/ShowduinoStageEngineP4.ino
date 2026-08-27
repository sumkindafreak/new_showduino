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
  - SHOW: timeline runtime (ShowRuntimeOwner)
  - HEARTBEAT response
  - Local USB Serial maintenance console (same command dispatcher as Comms UART)
*/

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include "driver/gpio.h"
#include "BoardConfig.h"
#include "ShowEngineState.h"
#include "ShowRuntimeOwner.h"
#include "src/StageStorage.h"
#include "src/StageAudio.h"
#include "src/EmergencyPixels.h"
#include "src/WebApiHandler.h"
#include "src/plugin/PluginBus.h"
#include "src/StageDiagnostics.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"

// -----------------------------
// Serial configuration
// -----------------------------
#define DEBUG_BAUD 115200

// Status LED pin. Change when the final P4 board pinout is chosen.
#define STATUS_LED_PIN 10

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

bool emergencyLocked = false;
uint8_t gEmergencySourceId = 0; /* 0 none, 1 director/comms, 2 physical, 3 local USB */
unsigned long lastHeartbeatMs = 0;

String inputBuffer = "";

static CommandSource sCmdSource = CommandSource::Comms;
static char sUsbLine[SHOWDUINO_COMMS_CMD_MAX + 1];
static uint16_t sUsbLen = 0;
static bool sUsbOverflow = false;

#if SHOWDUINO_ESTOP_GPIO >= 0
static int sEstopRaw = -1;
static int sEstopStable = -1;
static uint32_t sEstopEdgeMs = 0;
#endif

void triggerEmergency(EmergencySource source);
void clearEmergencyStop();
void handleCommand(String command, CommandSource source);
void handleCommand(String command);
void readCommsSerial();
void readUsbSerial();
void serviceCommsLink();

#if SHOWDUINO_ESTOP_GPIO >= 0
static bool physicalEstopAssertedNow() {
  /* Debounced hold check. A single noisy sample must not block Director CLEAR
   * after a momentary press that has already been released. */
  if (sEstopStable >= 0) {
    return sEstopStable != 0;
  }
  return digitalRead(SHOWDUINO_ESTOP_GPIO) == SHOWDUINO_ESTOP_ASSERTED_LEVEL;
}
#endif

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
  if (c.startsWith("SHOW:") || c.startsWith("AUDIO:") || c.startsWith("EMERGENCY:")) return true;
  if (c.startsWith("STATUS:") || c.startsWith("DMX:") || c.startsWith("PIXEL:")) return true;
  if (c.startsWith("PLUGIN:")) return true;
  if (c.startsWith("WEB/")) return true;
  if (c.startsWith("DIAG:")) return true;
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

static void noteCommsValidRx(const String &command) {
  const uint32_t now = millis();
  sCommsLastRxMs = now;
  if (!command.startsWith("DIAG:")) sCommsSawDirector = true;
  if (!sCommsLinkUp) {
    sCommsLinkUp = true;
    Serial.println(sCommsEverUp ? "[COMMS] Link restored" : "[COMMS] Link established");
    sCommsEverUp = true;
    if (command == "HEARTBEAT") {
      Serial.println("[COMMS] Heartbeat received");
    } else {
      Serial.print("[COMMS] RX: ");
      Serial.println(command);
    }
    return;
  }
  if (command == "HEARTBEAT") return;
  Serial.print("[COMMS] RX: ");
  Serial.println(command);
}

void serviceCommsLink() {
  if (!sCommsUartReady || !sCommsLinkUp) return;
  if ((millis() - sCommsLastRxMs) < SHOWDUINO_COMMS_LINK_TIMEOUT_MS) return;
  sCommsLinkUp = false;
  Serial.println("[COMMS] Link lost");
}

void sendToDirector(const String &message) {
  if (!sCommsUartReady || uartHushActive()) {
    return;
  }
  Serial1.println(message);
  Serial.print("[COMMS] TX: ");
  Serial.println(message);
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

bool stageEstopDebouncedAsserted() {
#if SHOWDUINO_ESTOP_GPIO >= 0
  return sEstopStable > 0;
#else
  return false;
#endif
}

static const char *commandSourceLabel(CommandSource source) {
  return (source == CommandSource::LocalUsb) ? "USB" : "COMMS";
}

/* ACK / ERR / STATUS replies for the requester. State broadcasts still use sendToDirector. */
static void sendCommandReply(const String &message) {
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
                  gpioRaw, sEstopStable, already ? "ACTIVE" : "CLEAR");
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
    return;
  }

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
  Serial.printf("[ESTOP] source_id=%u pixels=%s audio_wav=%s\n",
                (unsigned)gEmergencySourceId,
                emergencyPixelsReady() ? "ready" : "not-ready",
                stageAudioStatus().wavPresent ? "present" : "missing");

  stageAudioStopShow();
  gRuntime.onEmergencyStop(millis(), &gEngine);

  emergencyPixelsSetWhite();
  digitalWrite(STATUS_LED_PIN, HIGH);
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

void clearEmergencyStop() {
#if SHOWDUINO_ESTOP_GPIO >= 0
  if (physicalEstopAssertedNow()) {
    const int raw = digitalRead(SHOWDUINO_ESTOP_GPIO);
    Serial.printf("[ESTOP] Clear rejected: physical emergency button still asserted (gpio=%d stable=%d)\n",
                  raw, sEstopStable);
    sendCommandReply(SHOWDUINO_LEGACY_ERR_ESTOP_HELD);
    if (!emergencyLocked) {
      triggerEmergency(EmergencySource::Physical);
    } else if (sCmdSource != CommandSource::LocalUsb) {
      sendToDirector(SHOWDUINO_LEGACY_STATUS_ELOCKED);
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_ACTIVE);
    }
    return;
  }
#endif

  if (!emergencyLocked) {
    Serial.println("[ESTOP] CLEAR ignored — latch already clear");
    sendCommandReply(SHOWDUINO_LEGACY_STATUS_ECLEARED);
    if (sCmdSource != CommandSource::LocalUsb) {
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
    }
    return;
  }

  Serial.println("[ESTOP] CLEAR authorised — releasing latch");
  stageAudioStopEmergency();
  emergencyLocked = false;
  gEmergencySourceId = 0;
  gEngine.emergency = EmergencyState::Clear;
  showEngineBump(gEngine);
  emergencyPixelsBlackout();
  digitalWrite(STATUS_LED_PIN, LOW);
  /* Latch wire first so Director unlocks before the runtime mirror arrives. */
  sendCommandReply(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  if (sCmdSource == CommandSource::LocalUsb) {
    sendToDirector(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  }
  sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
  gRuntime.onEmergencyCleared(millis(), &gEngine);
  Serial.println("[ESTOP] Emergency cleared");
  Serial.println("[SHOW] Returning to safe idle state");
}

void sendCapabilities() {
  sendCommandReply("SHOWDUINO_STAGE_ENGINE");
  sendCommandReply("FW:0.2.0");
  sendCommandReply("DMX:PLANNED");
  sendCommandReply("PIXELS:PLANNED");
  sendCommandReply(stageAudioStatus().wavPresent ? "AUDIO:READY" : "AUDIO:PLANNED");
  sendCommandReply("INPUTS:PLANNED");
  sendCommandReply(stageStorageIsReady() ? "SD:READY" : "SD:PLANNED");
  sendCommandReply("READY");
}

static void printLocalConsoleStatus() {
  const StageAudioStatus &audio = stageAudioStatus();
  const StageStorageStatus &sd = stageStorageStatus();
  const char *prod = gRuntime.rt.showName[0] ? gRuntime.rt.showName : "(none)";
  Serial.printf("[CONSOLE] runtime=%s production=%s emergency=%s\n",
                showStateName(gRuntime.rt.state),
                prod,
                emergencyLocked ? "ACTIVE" : "CLEAR");
#if SHOWDUINO_ESTOP_GPIO >= 0
  Serial.printf("[CONSOLE] gpio25=%s stable=%s comms=%s director=%s\n",
                digitalRead(SHOWDUINO_ESTOP_GPIO) == SHOWDUINO_ESTOP_ASSERTED_LEVEL ? "ASSERTED" : "released",
                (sEstopStable > 0) ? "ASSERTED" : "released",
                sCommsLinkUp ? "ALIVE" : "DOWN",
                sCommsSawDirector ? "ONLINE" : "not-seen");
#else
  Serial.printf("[CONSOLE] gpio25=unassigned comms=%s director=%s\n",
                sCommsLinkUp ? "ALIVE" : "DOWN",
                sCommsSawDirector ? "ONLINE" : "not-seen");
#endif
  Serial.printf("[CONSOLE] sd=%s audio=%s emergency_audio=%s plugins=%u\n",
                stageStorageIsReady() ? "mounted" : sd.message,
                audio.i2sReady ? "ready" : "not-ready",
                audio.emergencyPlaying ? "playing" : (audio.wavPresent ? "file-present" : "file-missing"),
                (unsigned)pluginBusInstanceCount());
}

void sendStatus() {
  sendCommandReply(SHOWDUINO_WIRE_SNAPSHOT_BEGIN);
  sendCommandReply(emergencyLocked ? SHOWDUINO_LEGACY_STATUS_ELOCKED : "STATUS:READY");
  sendCommandReply(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) +
                   (emergencyLocked ? SHOWDUINO_WIRE_EMERGENCY_ACTIVE : SHOWDUINO_WIRE_EMERGENCY_CLEAR));
  sendCommandReply(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + showRuntimeWire(gEngine.show));
  sendCommandReply(SHOWDUINO_WIRE_SNAPSHOT_END);
  gRuntime.handleStateQuery();
  if (sCmdSource == CommandSource::LocalUsb) {
    printLocalConsoleStatus();
  }
}

void handleShowCommand(const String &command) {
  const uint32_t now = millis();

  if (command == "SHOW:STATE?") {
    gRuntime.handleStateQuery();
    return;
  }

  if (command == "SHOW:START" || command == "SHOW:RUN") {
    if (emergencyLocked) {
      Serial.println("[SHOW] Start rejected: EMERGENCY ACTIVE");
      sendCommandReply("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    gRuntime.handleRun(now, &gEngine);
    return;
  }

  if (command.startsWith("SHOW:RUN:") || command.startsWith("SHOW:LOAD:")) {
    int colon = command.indexOf(':', 5);
    String name = (colon >= 0) ? command.substring(colon + 1) : "";
    name.trim();
    if (!gRuntime.handleLoadName(name.c_str(), now, &gEngine)) return;
    return;
  }

  if (command == "SHOW:PAUSE") {
    gRuntime.handlePause(now, &gEngine);
    return;
  }

  if (command == "SHOW:RESUME") {
    gRuntime.handleResume(now, &gEngine);
    return;
  }

  if (command == "SHOW:STOP" || command == "STOP:ALL") {
    gRuntime.handleStop(now, &gEngine);
    if (!emergencyLocked) {
      stageAudioStopShow();
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
    gRuntime.handleTlEnd(now, &gEngine);
    return;
  }

  sendCommandReply("OK:SHOW:COMMAND_RECEIVED");
}

void handleAudioCommand(const String &command) {
  if (command.startsWith("AUDIO:LOCAL:PLAY") || command.startsWith("AUDIO:PLAY")) {
    if (emergencyLocked) {
      Serial.println("[SHOW] Start rejected: EMERGENCY ACTIVE");
      sendCommandReply("REJECTED:AUDIO:EMERGENCY_ACTIVE");
      return;
    }
    String path;
    if (command.startsWith("AUDIO:LOCAL:PLAY:")) {
      path = command.substring(strlen("AUDIO:LOCAL:PLAY:"));
    } else if (command.startsWith("AUDIO:PLAY:")) {
      path = command.substring(strlen("AUDIO:PLAY:"));
    }
    path.trim();
    if (path.length() == 0) {
      sendCommandReply("ERR:AUDIO:NO_PATH");
      return;
    }
    if (stageAudioStartShow(path.c_str())) {
      sendCommandReply("ACK:AUDIO:PLAY");
    } else {
      sendCommandReply("ERR:AUDIO:PLAY_FAILED");
    }
    return;
  }

  if (command == "AUDIO:LOCAL:STOP" || command == "AUDIO:STOP") {
    if (emergencyLocked) {
      sendCommandReply("REJECTED:AUDIO:EMERGENCY_ACTIVE");
      return;
    }
    stageAudioStopShow();
    sendCommandReply("ACK:AUDIO:STOP");
    return;
  }

  sendCommandReply("UNSUPPORTED:AUDIO");
}

static void printUsbHelp() {
  Serial.println("[CONSOLE] Showduino P4 commands:");
  Serial.println("  STATUS:REQUEST");
  Serial.println("  SHOW:START");
  Serial.println("  SHOW:STOP");
  Serial.println("  SHOW:PAUSE");
  Serial.println("  SHOW:RESUME");
  Serial.println("  SHOW:LOAD:<name>");
  Serial.println("  EMERGENCY:STOP");
  Serial.println("  EMERGENCY:CLEAR");
  Serial.println("  PLUGIN:SCAN");
  Serial.println("  PLUGIN:LIST");
  Serial.println("  PLUGIN:STATUS");
  Serial.println("  PLUGIN:INFO:<instance|address>");
  Serial.println("  RUN:TEST");
  Serial.println("  RUN:TEST:STATUS");
  Serial.println("  RUN:TEST:ABORT");
  Serial.println("  CONFIRM:PIXELS:YES | CONFIRM:PIXELS:NO");
  Serial.println("  CONFIRM:AUDIO:YES | CONFIRM:AUDIO:NO");
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

  if (command.startsWith("WEB/")) {
    webApiHandleTunnelRequest(command);
    return;
  }

  if (command == "HELLO") {
    sendCapabilities();
    return;
  }

  if (command == "HEARTBEAT") {
    if (sCmdSource != CommandSource::LocalUsb) {
      lastHeartbeatMs = millis();
    }
    sendCommandReply("OK:HEARTBEAT");
    return;
  }

  if (command == "STATUS:REQUEST") {
    sendStatus();
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
    clearEmergencyStop();
    return;
  }

  if (command.startsWith("SHOW:") || command == "STOP:ALL") {
    handleShowCommand(command);
    return;
  }

  if (command.startsWith("AUDIO:")) {
    handleAudioCommand(command);
    return;
  }

  if (command.startsWith("DMX:")) {
    sendCommandReply("UNSUPPORTED:DMX");
    return;
  }

  if (command.startsWith("PIXEL:")) {
    sendCommandReply("UNSUPPORTED:PIXEL");
    return;
  }

  if (command == "PLUGIN:SCAN") {
    Serial.println("[PLUGIN] Scanning Showduino Plug-in Bus...");
    pluginBusScan();
    pluginBusPrintList();
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

  sendCommandReply("ERR:UNKNOWN_COMMAND");
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
                            command.startsWith("PLUGIN:") ||
                            command.startsWith("RUN:TEST") ||
                            command.startsWith("CONFIRM:"));
    gRuntime.sendFn = localOnly ? emitConsoleLine : emitRuntimeToUsbAndDirector;
    if (command != "HEARTBEAT") {
  Serial.printf("[CMD] source=%s command=%s\n", commandSourceLabel(source), command.c_str());
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
#if SHOWDUINO_ESTOP_GPIO >= 0
  /* Momentary button: latch on a debounced press edge only.
   * Do not follow GPIO level. Do not retrigger while held.
   * Release (HIGH) and extra presses while already latched are ignored. */
  const int raw = digitalRead(SHOWDUINO_ESTOP_GPIO);
  const int pressed = (raw == SHOWDUINO_ESTOP_ASSERTED_LEVEL) ? 1 : 0;
  const uint32_t now = millis();

  if (pressed != sEstopRaw) {
    sEstopRaw = pressed;
    sEstopEdgeMs = now;
    Serial.printf("[ESTOP] gpio edge raw=%d (%s)\n", raw, pressed ? "LOW/press" : "HIGH/release");
  }
  if ((now - sEstopEdgeMs) < SHOWDUINO_ESTOP_DEBOUNCE_MS) return;
  if (sEstopStable == pressed) return;
  sEstopStable = pressed;
  Serial.printf("[ESTOP] GPIO%d %s (debounced)\n",
                SHOWDUINO_ESTOP_GPIO, pressed ? "PRESSED" : "RELEASED");
  if (pressed && !emergencyLocked) {
    triggerEmergency(EmergencySource::Physical);
  } else if (pressed && emergencyLocked) {
    Serial.println("[ESTOP] press ignored — already latched (CLEAR from Director)");
  }
#else
  (void)0;
#endif
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

  {
    struct timeval tv = {};
    if (gettimeofday(&tv, nullptr) == 0) {
      Serial.println("[RTC] Internal RTC initialised");
    } else {
      Serial.println("[RTC] Internal RTC not available");
    }
  }

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  /* Button before pixels. Adafruit NeoPixel show() could wait forever on P4 RMT
   * and previously blocked this pin from ever being configured. */
#if SHOWDUINO_ESTOP_GPIO >= 0
  gpio_reset_pin((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  gpio_set_direction((gpio_num_t)SHOWDUINO_ESTOP_GPIO, GPIO_MODE_INPUT);
  gpio_pullup_en((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  gpio_pulldown_dis((gpio_num_t)SHOWDUINO_ESTOP_GPIO);
  pinMode(SHOWDUINO_ESTOP_GPIO, SHOWDUINO_ESTOP_PIN_MODE);
  Serial.println("[ESTOP] Physical emergency input initialized");
  Serial.printf("[ESTOP] GPIO=%d mode=%s asserted=%s sample=%d (1=released)\n",
                SHOWDUINO_ESTOP_GPIO,
                SHOWDUINO_ESTOP_PIN_MODE == INPUT_PULLUP ? "INPUT_PULLUP" : "INPUT",
                SHOWDUINO_ESTOP_ASSERTED_LEVEL == LOW ? "LOW" : "HIGH",
                digitalRead(SHOWDUINO_ESTOP_GPIO));
#else
  Serial.println("[ESTOP] Emergency input initialized (command path only; GPIO not assigned)");
#endif

  emergencyPixelsBegin();

  stageStorageSetLinkPump(pumpLocalServices);
  Serial.println("[SD] Mounting SD card...");
  bool sdOk = stageStorageBegin();
  if (sdOk) {
    Serial.println("[SD] SD mounted");
    Serial.println("[SD] SD initialization success");
  } else {
    Serial.println("[SD] ERROR: SD card unavailable");
    Serial.println("[WEB] WebUI unavailable - SD not mounted");
  }

  gRuntime.begin(sendToDirectorC);
  gRuntime.setDispatch(timelineDispatchCommand);

  if (sdOk) {
    stageAudioBegin();
    if (!stageAudioStatus().wavPresent) {
      Serial.println("[ESTOP] WARNING: Emergency audio unavailable");
      Serial.println("[ESTOP] Emergency safety state remains operational");
    }
    Serial.println("[AUDIO] Audio subsystem ready");
  } else {
    Serial.println("[ESTOP] WARNING: Emergency audio unavailable");
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

  webApiBegin(bootMs);

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
  servicePhysicalEstop();
  pluginBusService();
  stageStorageLoop();
  stageAudioLoop();
  gRuntime.service(millis(), &gEngine);

  if (emergencyLocked) {
    digitalWrite(STATUS_LED_PIN, (millis() / 250) % 2 == 0 ? HIGH : LOW);
    emergencyPixelsService();
  }
  stageDiagService();
}
