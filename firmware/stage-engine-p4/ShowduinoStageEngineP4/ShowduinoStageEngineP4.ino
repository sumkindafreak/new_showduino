/*
  Showduino Stage Engine - ESP32-P4

  Role:
  - Replaces the old Arduino Mega executor role.
  - Receives simple text commands from the Director via SUE UART.
  - Executes hardware actions safely and reports status back.

  Current features:
  - HELLO capability handshake
  - STATUS:REQUEST response
  - Latched EMERGENCY:STOP / EMERGENCY:CLEAR
  - Physical E-stop GPIO (when assigned in BoardConfig.h)
  - Emergency audio loop from SD
  - SHOW: timeline runtime (ShowRuntimeOwner)
  - HEARTBEAT response

  Board note:
  - ESP32-P4 Arduino support is still board-package dependent.
  - UART pins below are the active sketch mapping (not guessed E-stop GPIO).
*/

#include <Arduino.h>
#include "driver/gpio.h"
#include "BoardConfig.h"
#include "ShowEngineState.h"
#include "ShowRuntimeOwner.h"
#include "src/StageStorage.h"
#include "src/StageAudio.h"
#include "src/EmergencyPixels.h"
#include "src/WebApiHandler.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"

// -----------------------------
// Serial configuration
// -----------------------------
#define DEBUG_BAUD 115200
#define DIRECTOR_BAUD 115200

// UART pins between SUE / Director path and Stage Engine ESP32-P4.
#define DIRECTOR_RX_PIN 18
#define DIRECTOR_TX_PIN 17

// Status LED pin. Change when the final P4 board pinout is chosen.
#define STATUS_LED_PIN 10

enum class EmergencySource : uint8_t {
  Director = 0,
  Physical
};

// -----------------------------
// Runtime state
// -----------------------------
ShowEngineState gEngine;
ShowRuntimeOwner gRuntime;

bool emergencyLocked = false;
uint8_t gEmergencySourceId = 0; /* 0 none, 1 director, 2 physical */
unsigned long lastHeartbeatMs = 0;

String inputBuffer = "";

#if SHOWDUINO_ESTOP_GPIO >= 0
static int sEstopRaw = -1;
static int sEstopStable = -1;
static uint32_t sEstopEdgeMs = 0;
#endif

void triggerEmergency(EmergencySource source);
void clearEmergencyStop();

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
// Helper functions
// -----------------------------
void sendToDirector(const String &message) {
  Serial1.println(message);
  Serial.print("TX -> Director: ");
  Serial.println(message);
}

static void sendToDirectorC(const char *line) {
  if (line && line[0]) sendToDirector(String(line));
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
    Serial.printf("[ESTOP] TRIGGER source=PHYSICAL gpio=%d stable=%d latch=%s\n",
                  gpioRaw, sEstopStable, already ? "ACTIVE" : "CLEAR");
  } else {
    Serial.printf("[ESTOP] TRIGGER source=DIRECTOR gpio=%d latch=%s cmd=EMERGENCY:STOP\n",
                  gpioRaw, already ? "ACTIVE" : "CLEAR");
  }

  if (already) {
    Serial.println("[ESTOP] already latched — extra trigger ignored");
    return;
  }

  emergencyLocked = true;
  gEmergencySourceId = (source == EmergencySource::Physical) ? 2 : 1;
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
    sendToDirector(SHOWDUINO_LEGACY_ERR_ESTOP_HELD);
    if (!emergencyLocked) {
      triggerEmergency(EmergencySource::Physical);
    } else {
      sendToDirector(SHOWDUINO_LEGACY_STATUS_ELOCKED);
      sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_ACTIVE);
    }
    return;
  }
#endif

  if (!emergencyLocked) {
    Serial.println("[ESTOP] CLEAR ignored — latch already clear");
    sendToDirector(SHOWDUINO_LEGACY_STATUS_ECLEARED);
    sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
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
  sendToDirector(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + SHOWDUINO_WIRE_EMERGENCY_CLEAR);
  gRuntime.onEmergencyCleared(millis(), &gEngine);
  Serial.println("[ESTOP] Emergency cleared");
  Serial.println("[SHOW] Returning to safe idle state");
}

void sendCapabilities() {
  sendToDirector("SHOWDUINO_STAGE_ENGINE");
  sendToDirector("FW:0.2.0");
  sendToDirector("DMX:PLANNED");
  sendToDirector("PIXELS:PLANNED");
  sendToDirector(stageAudioStatus().wavPresent ? "AUDIO:READY" : "AUDIO:PLANNED");
  sendToDirector("INPUTS:PLANNED");
  sendToDirector(stageStorageIsReady() ? "SD:READY" : "SD:PLANNED");
  sendToDirector("READY");
}

void sendStatus() {
  sendToDirector(SHOWDUINO_WIRE_SNAPSHOT_BEGIN);
  sendToDirector(emergencyLocked ? SHOWDUINO_LEGACY_STATUS_ELOCKED : "STATUS:READY");
  sendToDirector(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) +
                 (emergencyLocked ? SHOWDUINO_WIRE_EMERGENCY_ACTIVE : SHOWDUINO_WIRE_EMERGENCY_CLEAR));
  sendToDirector(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + showRuntimeWire(gEngine.show));
  sendToDirector(SHOWDUINO_WIRE_SNAPSHOT_END);
  gRuntime.handleStateQuery();
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
      sendToDirector("REJECTED:SHOW:EMERGENCY_ACTIVE");
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
      sendToDirector("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    gRuntime.handleTlBegin();
    return;
  }

  if (command.startsWith("SHOW:TL:C:")) {
    if (emergencyLocked) {
      sendToDirector("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    String rest = command.substring(strlen("SHOW:TL:C:"));
    int colon = rest.indexOf(':');
    uint32_t t = (colon >= 0) ? (uint32_t)rest.substring(0, colon).toInt() : rest.toInt();
    String cmd = (colon >= 0) ? rest.substring(colon + 1) : "";
    if (!gRuntime.handleTlCue(t, cmd.c_str())) {
      sendToDirector("ERR:SHOW:TL:C");
    }
    return;
  }

  if (command == "SHOW:TL:END") {
    if (emergencyLocked) {
      sendToDirector("REJECTED:SHOW:EMERGENCY_ACTIVE");
      return;
    }
    gRuntime.handleTlEnd(now, &gEngine);
    return;
  }

  sendToDirector("OK:SHOW:COMMAND_RECEIVED");
}

void handleAudioCommand(const String &command) {
  if (command.startsWith("AUDIO:LOCAL:PLAY") || command.startsWith("AUDIO:PLAY")) {
    if (emergencyLocked) {
      Serial.println("[SHOW] Start rejected: EMERGENCY ACTIVE");
      sendToDirector("REJECTED:AUDIO:EMERGENCY_ACTIVE");
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
      sendToDirector("ERR:AUDIO:NO_PATH");
      return;
    }
    if (stageAudioStartShow(path.c_str())) {
      sendToDirector("ACK:AUDIO:PLAY");
    } else {
      sendToDirector("ERR:AUDIO:PLAY_FAILED");
    }
    return;
  }

  if (command == "AUDIO:LOCAL:STOP" || command == "AUDIO:STOP") {
    if (emergencyLocked) {
      sendToDirector("REJECTED:AUDIO:EMERGENCY_ACTIVE");
      return;
    }
    stageAudioStopShow();
    sendToDirector("ACK:AUDIO:STOP");
    return;
  }

  sendToDirector("UNSUPPORTED:AUDIO");
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith("WEB/")) {
    webApiHandleTunnelRequest(command);
    return;
  }

  Serial.print("RX <- Director: ");
  Serial.println(command);

  if (command == "HELLO") {
    sendCapabilities();
    return;
  }

  if (command == "HEARTBEAT") {
    lastHeartbeatMs = millis();
    sendToDirector("OK:HEARTBEAT");
    return;
  }

  if (command == "STATUS:REQUEST") {
    sendStatus();
    return;
  }

  if (command == "EMERGENCY:STOP") {
    Serial.println("[ESTOP] UART cmd EMERGENCY:STOP");
    triggerEmergency(EmergencySource::Director);
    return;
  }

  if (command == "EMERGENCY:CLEAR") {
    Serial.println("[ESTOP] UART cmd EMERGENCY:CLEAR");
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
    sendToDirector("UNSUPPORTED:DMX");
    return;
  }

  if (command.startsWith("PIXEL:")) {
    sendToDirector("UNSUPPORTED:PIXEL");
    return;
  }

  sendToDirector("ERR:UNKNOWN_COMMAND");
}

static void timelineDispatchCommand(const char *command) {
  if (!command || !command[0]) return;
  handleCommand(String(command));
}

void readDirectorSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        handleCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;

      if (inputBuffer.length() > 240) {
        inputBuffer = "";
        sendToDirector("ERR:COMMAND_TOO_LONG");
      }
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

  static uint32_t sEstopHbMs = 0;
  if ((now - sEstopHbMs) >= 2000UL) {
    sEstopHbMs = now;
    Serial.printf("[ESTOP] poll gpio=%d raw=%d %s stable=%d latch=%s src=%u\n",
                  SHOWDUINO_ESTOP_GPIO,
                  raw,
                  pressed ? "PRESSED" : "released",
                  sEstopStable,
                  emergencyLocked ? "ACTIVE" : "CLEAR",
                  (unsigned)gEmergencySourceId);
  }

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

void setup() {
  unsigned long bootMs = millis();
  Serial.begin(DEBUG_BAUD);
  delay(200);

  Serial.println();
  Serial.println("[BOOT] Showduino P4 starting");

  /* UART to SUE must be live before SD/audio. Director HELLO dies if setup blocks. */
  Serial1.setRxBufferSize(1024);
  Serial1.begin(DIRECTOR_BAUD, SERIAL_8N1, DIRECTOR_RX_PIN, DIRECTOR_TX_PIN);
  stageStorageSetLinkPump(readDirectorSerial);
  Serial.println("[NET] UART link to SUE ready");
  sendToDirector("BOOT:STAGE_ENGINE_READY");
  readDirectorSerial();

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
  readDirectorSerial();

  gRuntime.begin(sendToDirectorC);
  gRuntime.setDispatch(timelineDispatchCommand);
  gRuntime.bootToIdle();
  readDirectorSerial();

  Serial.println("[SD] Mounting SD card...");
  bool sdOk = stageStorageBegin();
  readDirectorSerial();
  if (sdOk) {
    Serial.println("[SD] SD mounted");
    Serial.println("[SD] SD initialization success");
    stageAudioBegin();
    if (!stageAudioStatus().wavPresent) {
      Serial.println("[ESTOP] WARNING: Emergency audio unavailable");
      Serial.println("[ESTOP] Emergency safety state remains operational");
    }
    Serial.println("[AUDIO] Audio subsystem ready");
  } else {
    Serial.println("[SD] ERROR: SD card unavailable");
    Serial.println("[WEB] WebUI unavailable - SD not mounted");
    Serial.println("[ESTOP] WARNING: Emergency audio unavailable");
    Serial.println("[ESTOP] Emergency safety state remains operational");
  }

  webApiBegin(bootMs);
  Serial.println("[NET] Network ready");
  Serial.println("[WEB] HTTP server started");

  lastHeartbeatMs = millis();
  Serial.println("[SYSTEM] Showduino ready");
  sendToDirector("BOOT:STAGE_ENGINE_READY");
  readDirectorSerial();
}

void loop() {
  readDirectorSerial();
  servicePhysicalEstop();
  stageStorageLoop();
  stageAudioLoop();
  gRuntime.service(millis(), &gEngine);

  if (emergencyLocked) {
    digitalWrite(STATUS_LED_PIN, (millis() / 250) % 2 == 0 ? HIGH : LOW);
    emergencyPixelsService();
  }
}
