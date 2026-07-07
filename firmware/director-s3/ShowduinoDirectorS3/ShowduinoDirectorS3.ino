/*
  Showduino Director - ESP32-S3

  Role:
  - Serves as the touchscreen and WebUI side of Showduino.
  - Talks to the ESP32-P4 Stage Engine over UART.
  - For this starter build, Serial Monitor acts as the WebUI/touchscreen command source.

  Current starter features:
  - Sends HELLO to Stage Engine on boot
  - Sends HEARTBEAT every second
  - Forwards Serial Monitor commands to the Stage Engine
  - Prints Stage Engine replies to Serial Monitor

  Type commands into Serial Monitor at 115200 baud:

    HELLO
    STATUS:REQUEST
    RELAY:1:ON
    RELAY:1:OFF
    RELAY:1:PULSE:1000
    EMERGENCY:STOP
    EMERGENCY:CLEAR
*/

#include <Arduino.h>

// -----------------------------
// Serial configuration
// -----------------------------
#define USB_DEBUG_BAUD 115200
#define STAGE_ENGINE_BAUD 115200

// UART pins between Director ESP32-S3 and Stage Engine ESP32-P4.
// Change these once the final wiring is chosen.
#define STAGE_ENGINE_RX_PIN 19
#define STAGE_ENGINE_TX_PIN 20

// -----------------------------
// Runtime config
// -----------------------------
#define HEARTBEAT_INTERVAL_MS 1000UL
#define HELLO_RETRY_INTERVAL_MS 5000UL

// -----------------------------
// Runtime state
// -----------------------------
String usbInputBuffer = "";
String stageInputBuffer = "";

unsigned long lastHeartbeatMs = 0;
unsigned long lastHelloMs = 0;
bool stageReady = false;

// -----------------------------
// Helper functions
// -----------------------------
void sendToStage(const String &command) {
  Serial1.println(command);
  Serial.print("TX -> Stage: ");
  Serial.println(command);
}

void handleStageLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("RX <- Stage: ");
  Serial.println(line);

  if (line == "READY") {
    stageReady = true;
    Serial.println("Director status: Stage Engine is READY.");
  }

  if (line == "STATUS:EMERGENCY_LOCKED") {
    Serial.println("Director warning: Stage Engine emergency lock is active.");
  }

  if (line == "STATUS:EMERGENCY_CLEARED") {
    Serial.println("Director status: Emergency lock cleared.");
  }
}

void readStageSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\n' || c == '\r') {
      if (stageInputBuffer.length() > 0) {
        handleStageLine(stageInputBuffer);
        stageInputBuffer = "";
      }
    } else {
      stageInputBuffer += c;

      if (stageInputBuffer.length() > 200) {
        stageInputBuffer = "";
        Serial.println("Director warning: Stage response too long, buffer cleared.");
      }
    }
  }
}

void handleUsbLine(String command) {
  command.trim();
  if (command.length() == 0) return;

  Serial.print("USB command: ");
  Serial.println(command);

  if (command == "HELP") {
    Serial.println("Available starter commands:");
    Serial.println("  HELLO");
    Serial.println("  STATUS:REQUEST");
    Serial.println("  RELAY:1:ON");
    Serial.println("  RELAY:1:OFF");
    Serial.println("  RELAY:1:PULSE:1000");
    Serial.println("  EMERGENCY:STOP");
    Serial.println("  EMERGENCY:CLEAR");
    return;
  }

  sendToStage(command);
}

void readUsbSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (usbInputBuffer.length() > 0) {
        handleUsbLine(usbInputBuffer);
        usbInputBuffer = "";
      }
    } else {
      usbInputBuffer += c;

      if (usbInputBuffer.length() > 160) {
        usbInputBuffer = "";
        Serial.println("USB command too long, buffer cleared.");
      }
    }
  }
}

void sendHeartbeatIfDue() {
  unsigned long now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendToStage("HEARTBEAT");
  }
}

void sendHelloIfNeeded() {
  if (stageReady) return;

  unsigned long now = millis();
  if (now - lastHelloMs >= HELLO_RETRY_INTERVAL_MS) {
    lastHelloMs = now;
    sendToStage("HELLO");
  }
}

void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino Director ESP32-S3 starting...");
  Serial.println("Type HELP for starter commands.");

  Serial1.begin(STAGE_ENGINE_BAUD, SERIAL_8N1, STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN);

  lastHeartbeatMs = millis();
  lastHelloMs = 0;

  sendToStage("HELLO");
}

void loop() {
  readUsbSerial();
  readStageSerial();
  sendHeartbeatIfDue();
  sendHelloIfNeeded();
}
