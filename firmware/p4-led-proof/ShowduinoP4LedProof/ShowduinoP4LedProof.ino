/*
  Showduino P4 LED Proof-of-Life

  Purpose:
  - Runs on the ESP32-P4 side.
  - Receives text commands from the built-in ESP32-C6 bridge over UART.
  - Toggles a test LED/output when the touchscreen sends LED:TOGGLE.

  Tonight's test chain:

    5in touchscreen button
      -> ESP-NOW
      -> built-in C6 bridge
      -> UART
      -> P4 LED/output toggles

  Serial Monitor: 115200 baud

  IMPORTANT:
  - Set TEST_LED_PIN to a real safe output on your P4 board.
  - If your P4 board has a labelled built-in LED, use that pin.
  - If not, connect an LED + resistor to TEST_LED_PIN and GND.
*/

#include <Arduino.h>

// =========================================================
// Pin and serial settings
// =========================================================
#define USB_DEBUG_BAUD 115200
#define C6_UART_BAUD   115200

// Confirm these against your exact P4 board.
// These must match the C6 bridge sketch P4_UART_RX_PIN / P4_UART_TX_PIN wiring.
#define C6_UART_RX_PIN 4
#define C6_UART_TX_PIN 5

// Change this to your P4 built-in LED pin when confirmed.
// If unsure, wire a normal LED + resistor to this pin for the first proof test.
#define TEST_LED_PIN 2
#define TEST_LED_ON  HIGH
#define TEST_LED_OFF LOW

// =========================================================
// Global variables
// =========================================================
String c6InputBuffer;
String usbInputBuffer;
bool ledState = false;
uint32_t commandCount = 0;
unsigned long lastCommandMs = 0;

// =========================================================
// LED helpers
// =========================================================
void applyLedState() {
  digitalWrite(TEST_LED_PIN, ledState ? TEST_LED_ON : TEST_LED_OFF);
  Serial.print("LED state: ");
  Serial.println(ledState ? "ON" : "OFF");
}

void toggleLed() {
  ledState = !ledState;
  applyLedState();
}

// =========================================================
// Command parser
// =========================================================
void handleCommand(String command, const char *source) {
  command.trim();
  if (command.length() == 0) return;

  commandCount++;
  lastCommandMs = millis();

  Serial.print("RX <- ");
  Serial.print(source);
  Serial.print(": ");
  Serial.println(command);

  if (command == "HELLO") {
    Serial1.println("READY");
    Serial.println("TX -> C6: READY");
    return;
  }

  if (command == "STATUS:REQUEST") {
    Serial1.print("STATUS:P4_LED_PROOF:");
    Serial1.println(ledState ? "LED_ON" : "LED_OFF");
    Serial.println("TX -> C6: STATUS response sent");
    return;
  }

  if (command == "LED:ON") {
    ledState = true;
    applyLedState();
    Serial1.println("ACK:LED:ON");
    return;
  }

  if (command == "LED:OFF") {
    ledState = false;
    applyLedState();
    Serial1.println("ACK:LED:OFF");
    return;
  }

  if (command == "LED:TOGGLE") {
    toggleLed();
    Serial1.println("ACK:LED:TOGGLE");
    return;
  }

  if (command == "EMERGENCY:STOP") {
    ledState = false;
    applyLedState();
    Serial1.println("STATUS:EMERGENCY_LOCKED");
    return;
  }

  Serial1.print("WARN:UNKNOWN_COMMAND:");
  Serial1.println(command);
}

// =========================================================
// Read C6 bridge UART
// =========================================================
void readC6Serial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (c6InputBuffer.length() > 0) {
        handleCommand(c6InputBuffer, "C6");
        c6InputBuffer = "";
      }
    } else {
      c6InputBuffer += c;
      if (c6InputBuffer.length() > 180) {
        c6InputBuffer = "";
        Serial.println("C6 command too long; buffer cleared.");
      }
    }
  }
}

// =========================================================
// USB bench commands
// =========================================================
void handleUsbLine(String command) {
  command.trim();
  if (command == "HELP") {
    Serial.println("Commands: HELP, LED:ON, LED:OFF, LED:TOGGLE, STATUS:REQUEST, HELLO");
    return;
  }
  handleCommand(command, "USB");
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
      if (usbInputBuffer.length() > 180) {
        usbInputBuffer = "";
        Serial.println("USB command too long; buffer cleared.");
      }
    }
  }
}

// =========================================================
// Setup
// =========================================================
void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino P4 LED Proof-of-Life starting...");

  pinMode(TEST_LED_PIN, OUTPUT);
  ledState = false;
  applyLedState();

  Serial1.begin(C6_UART_BAUD, SERIAL_8N1, C6_UART_RX_PIN, C6_UART_TX_PIN);
  Serial.printf("C6 UART: RX=%d TX=%d baud=%d\n", C6_UART_RX_PIN, C6_UART_TX_PIN, C6_UART_BAUD);
  Serial.printf("Test LED pin: %d\n", TEST_LED_PIN);

  Serial.println("Ready. Type HELP for bench commands.");
  Serial1.println("READY");
}

// =========================================================
// Main loop
// =========================================================
void loop() {
  readUsbSerial();
  readC6Serial();
  delay(5);
}
