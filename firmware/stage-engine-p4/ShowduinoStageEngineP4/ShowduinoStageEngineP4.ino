/*
  Showduino Stage Engine - ESP32-P4

  Role:
  - Replaces the old Arduino Mega executor role.
  - Receives simple text commands from the ESP32-S3 Director over UART.
  - Executes hardware actions safely and reports status back.

  Current starter features:
  - HELLO capability handshake
  - STATUS:REQUEST response
  - EMERGENCY:STOP / EMERGENCY:CLEAR lockout
  - RELAY:<channel>:ON
  - RELAY:<channel>:OFF
  - RELAY:<channel>:PULSE:<milliseconds>
  - HEARTBEAT response

  Board note:
  - ESP32-P4 Arduino support is still board-package dependent.
  - Pin numbers below are starter placeholders for the Stage Engine PCB/pinout stage.
  - Update these pin definitions once the real ESP32-P4 board wiring is chosen.
*/

#include <Arduino.h>

// -----------------------------
// Serial configuration
// -----------------------------
#define DEBUG_BAUD 115200
#define DIRECTOR_BAUD 115200

// UART pins between Director ESP32-S3 and Stage Engine ESP32-P4.
// Change these once the final wiring is chosen.
#define DIRECTOR_RX_PIN 18
#define DIRECTOR_TX_PIN 17

// -----------------------------
// Stage Engine hardware config
// -----------------------------
#define RELAY_COUNT 8

// Starter relay pins. These must be changed to match the real ESP32-P4 board/PCB.
const uint8_t RELAY_PINS[RELAY_COUNT] = { 2, 3, 4, 5, 6, 7, 8, 9 };

// Most relay modules are active LOW. Set to false if your board uses active HIGH relays.
#define RELAY_ACTIVE_LOW true

// Status LED pin. Change when the final P4 board pinout is chosen.
#define STATUS_LED_PIN 10

// -----------------------------
// Runtime state
// -----------------------------
bool emergencyLocked = false;
bool relays[RELAY_COUNT] = { false, false, false, false, false, false, false, false };
unsigned long lastHeartbeatMs = 0;

String inputBuffer = "";

// -----------------------------
// Helper functions
// -----------------------------
void sendToDirector(const String &message) {
  Serial1.println(message);
  Serial.print("TX -> Director: ");
  Serial.println(message);
}

void setRelay(uint8_t channel, bool on) {
  if (channel < 1 || channel > RELAY_COUNT) {
    sendToDirector("ERR:INVALID_RELAY_CHANNEL");
    return;
  }

  uint8_t index = channel - 1;
  relays[index] = on;

  bool outputLevel = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(RELAY_PINS[index], outputLevel ? HIGH : LOW);

  sendToDirector(String("OK:RELAY:") + channel + (on ? ":ON" : ":OFF"));
}

void allRelaysOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    relays[i] = false;
    bool outputLevel = RELAY_ACTIVE_LOW ? true : false;
    digitalWrite(RELAY_PINS[i], outputLevel ? HIGH : LOW);
  }
  sendToDirector("OK:RELAY:ALL:OFF");
}

void pulseRelay(uint8_t channel, unsigned long pulseMs) {
  if (channel < 1 || channel > RELAY_COUNT) {
    sendToDirector("ERR:INVALID_RELAY_CHANNEL");
    return;
  }

  if (pulseMs == 0 || pulseMs > 60000UL) {
    sendToDirector("ERR:INVALID_PULSE_TIME");
    return;
  }

  setRelay(channel, true);
  delay(pulseMs);
  setRelay(channel, false);
  sendToDirector(String("OK:RELAY:") + channel + ":PULSE:" + pulseMs);
}

void enterEmergencyStop() {
  emergencyLocked = true;
  allRelaysOff();
  digitalWrite(STATUS_LED_PIN, HIGH);
  sendToDirector("STATUS:EMERGENCY_LOCKED");
}

void clearEmergencyStop() {
  emergencyLocked = false;
  digitalWrite(STATUS_LED_PIN, LOW);
  sendToDirector("STATUS:EMERGENCY_CLEARED");
}

void sendCapabilities() {
  sendToDirector("SHOWDUINO_STAGE_ENGINE");
  sendToDirector("FW:0.1.0");
  sendToDirector(String("RELAYS:") + RELAY_COUNT);
  sendToDirector("DMX:PLANNED");
  sendToDirector("PIXELS:PLANNED");
  sendToDirector("AUDIO:PLANNED");
  sendToDirector("INPUTS:PLANNED");
  sendToDirector("SD:PLANNED");
  sendToDirector("READY");
}

void sendStatus() {
  String status = emergencyLocked ? "STATUS:EMERGENCY_LOCKED" : "STATUS:READY";
  sendToDirector(status);

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    sendToDirector(String("RELAY:") + (i + 1) + (relays[i] ? ":ON" : ":OFF"));
  }
}

int getTokenIndex(const String &command, uint8_t tokenIndex) {
  int start = 0;
  int currentToken = 0;

  while (currentToken < tokenIndex) {
    start = command.indexOf(':', start);
    if (start < 0) return -1;
    start++;
    currentToken++;
  }

  return start;
}

String getToken(const String &command, uint8_t tokenIndex) {
  int start = getTokenIndex(command, tokenIndex);
  if (start < 0) return "";

  int end = command.indexOf(':', start);
  if (end < 0) end = command.length();

  return command.substring(start, end);
}

void handleRelayCommand(const String &command) {
  String channelToken = getToken(command, 1);
  String action = getToken(command, 2);

  if (channelToken == "ALL" && action == "OFF") {
    allRelaysOff();
    return;
  }

  uint8_t channel = channelToken.toInt();

  if (action == "ON") {
    if (emergencyLocked) {
      sendToDirector("ERR:EMERGENCY_LOCKED");
      return;
    }
    setRelay(channel, true);
    return;
  }

  if (action == "OFF") {
    setRelay(channel, false);
    return;
  }

  if (action == "PULSE") {
    if (emergencyLocked) {
      sendToDirector("ERR:EMERGENCY_LOCKED");
      return;
    }
    unsigned long pulseMs = getToken(command, 3).toInt();
    pulseRelay(channel, pulseMs);
    return;
  }

  sendToDirector("ERR:UNKNOWN_RELAY_ACTION");
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

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
    enterEmergencyStop();
    return;
  }

  if (command == "EMERGENCY:CLEAR") {
    clearEmergencyStop();
    return;
  }

  if (command.startsWith("RELAY:")) {
    handleRelayCommand(command);
    return;
  }

  if (command.startsWith("SHOW:")) {
    sendToDirector("OK:SHOW:COMMAND_RECEIVED");
    return;
  }

  if (command.startsWith("AUDIO:")) {
    sendToDirector("OK:AUDIO:COMMAND_STUBBED");
    return;
  }

  if (command.startsWith("DMX:")) {
    sendToDirector("OK:DMX:COMMAND_STUBBED");
    return;
  }

  if (command.startsWith("PIXEL:")) {
    sendToDirector("OK:PIXEL:COMMAND_STUBBED");
    return;
  }

  sendToDirector("ERR:UNKNOWN_COMMAND");
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

      if (inputBuffer.length() > 160) {
        inputBuffer = "";
        sendToDirector("ERR:COMMAND_TOO_LONG");
      }
    }
  }
}

void setupRelays() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    bool outputLevel = RELAY_ACTIVE_LOW ? true : false;
    digitalWrite(RELAY_PINS[i], outputLevel ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino Stage Engine ESP32-P4 starting...");

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  setupRelays();

  Serial1.begin(DIRECTOR_BAUD, SERIAL_8N1, DIRECTOR_RX_PIN, DIRECTOR_TX_PIN);

  lastHeartbeatMs = millis();

  Serial.println("Stage Engine ready. Waiting for Director HELLO...");
  sendToDirector("BOOT:STAGE_ENGINE_READY");
}

void loop() {
  readDirectorSerial();

  // Slow blink while emergency locked so it is obvious on the bench.
  if (emergencyLocked) {
    digitalWrite(STATUS_LED_PIN, (millis() / 250) % 2 == 0 ? HIGH : LOW);
  }
}
