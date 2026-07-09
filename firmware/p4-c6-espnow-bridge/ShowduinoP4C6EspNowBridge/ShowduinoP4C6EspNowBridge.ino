/*
  Showduino P4 Built-in C6 ESP-NOW Bridge

  Purpose:
  - Runs on the ESP32-C6 wireless side of a P4 board.
  - Receives ESP-NOW commands from the portable 5" ESP32-S3 Director.
  - Forwards clean text commands to the ESP32-P4 over UART.

  Required Arduino libraries:
  - ESP32 Arduino core with ESP32-C6 support
  - Built-in libraries: WiFi, esp_now

  Serial Monitor: 115200 baud

  IMPORTANT:
  - Set P4_UART_RX_PIN and P4_UART_TX_PIN to match your exact P4+C6 board wiring.
  - The sketch prints the C6 MAC address at boot. Copy that MAC into the Director's BoardConfig.h.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// =========================================================
// Pin and serial configuration
// =========================================================
#define USB_DEBUG_BAUD 115200
#define P4_UART_BAUD   115200

// Confirm these once you check the P4/C6 board schematic or pinout.
#define P4_UART_RX_PIN 4
#define P4_UART_TX_PIN 5

// ESP-NOW settings must match the portable Director.
#define SHOWDUINO_ESPNOW_CHANNEL 1
#define SHOWDUINO_ESPNOW_MAGIC 0x5348444FUL
#define SHOWDUINO_ESPNOW_VERSION 1
#define SHOWDUINO_ESPNOW_COMMAND_MAX 96

// =========================================================
// Packet format matching the portable Director firmware
// =========================================================
struct ShowduinoEspNowPacket {
  uint32_t magic;
  uint16_t version;
  uint16_t sequence;
  uint32_t sentMillis;
  char command[SHOWDUINO_ESPNOW_COMMAND_MAX];
};

// =========================================================
// State variables
// =========================================================
uint32_t receivedPackets = 0;
uint32_t rejectedPackets = 0;
uint16_t lastSequence = 0;
unsigned long lastPacketMs = 0;

// =========================================================
// Helper: print a MAC address in a readable format
// =========================================================
void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
}

// =========================================================
// Forward command to the P4 host
// =========================================================
void forwardToP4(const char *command) {
  Serial1.println(command);
  Serial.print("TX -> P4: ");
  Serial.println(command);
}

// =========================================================
// ESP-NOW receive callback
// =========================================================
void onEspNowReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
  if (len != sizeof(ShowduinoEspNowPacket)) {
    rejectedPackets++;
    Serial.printf("ESP-NOW: rejected packet, bad length %d\n", len);
    return;
  }

  ShowduinoEspNowPacket packet = {};
  memcpy(&packet, incomingData, sizeof(packet));

  if (packet.magic != SHOWDUINO_ESPNOW_MAGIC || packet.version != SHOWDUINO_ESPNOW_VERSION) {
    rejectedPackets++;
    Serial.println("ESP-NOW: rejected packet, bad magic/version.");
    return;
  }

  packet.command[SHOWDUINO_ESPNOW_COMMAND_MAX - 1] = '\0';
  receivedPackets++;
  lastSequence = packet.sequence;
  lastPacketMs = millis();

  Serial.print("RX <- Director ");
  if (recvInfo != nullptr) printMac(recvInfo->src_addr);
  Serial.print(" seq=");
  Serial.print(packet.sequence);
  Serial.print(" cmd=");
  Serial.println(packet.command);

  forwardToP4(packet.command);
}

// =========================================================
// USB debug command handling
// =========================================================
String usbBuffer;

void handleUsbLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "HELP") {
    Serial.println("Commands: HELP, STATUS, TEST:HELLO, or type any P4 command to forward it.");
    return;
  }

  if (line == "STATUS") {
    Serial.println("--- Showduino P4/C6 Bridge Status ---");
    Serial.print("C6 MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("Received: %lu\n", (unsigned long)receivedPackets);
    Serial.printf("Rejected: %lu\n", (unsigned long)rejectedPackets);
    Serial.printf("Last sequence: %u\n", lastSequence);
    Serial.printf("Last packet age: %lums\n", lastPacketMs == 0 ? 0UL : millis() - lastPacketMs);
    return;
  }

  if (line == "TEST:HELLO") {
    forwardToP4("HELLO");
    return;
  }

  forwardToP4(line.c_str());
}

void readUsbSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbBuffer.length() > 0) {
        handleUsbLine(usbBuffer);
        usbBuffer = "";
      }
    } else {
      usbBuffer += c;
      if (usbBuffer.length() > 180) {
        usbBuffer = "";
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
  Serial.println("Showduino P4 built-in C6 ESP-NOW Bridge starting...");

  Serial1.begin(P4_UART_BAUD, SERIAL_8N1, P4_UART_RX_PIN, P4_UART_TX_PIN);
  Serial.printf("P4 UART: RX=%d TX=%d baud=%d\n", P4_UART_RX_PIN, P4_UART_TX_PIN, P4_UART_BAUD);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  Serial.print("C6 MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Copy this MAC into the Director BoardConfig.h P4/C6 peer fields.");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: init failed. Halting bridge.");
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("ESP-NOW: receiver ready.");
  Serial.println("Type HELP for USB debug commands.");
}

// =========================================================
// Main loop
// =========================================================
void loop() {
  readUsbSerial();

  while (Serial1.available() > 0) {
    String p4Line = Serial1.readStringUntil('\n');
    p4Line.trim();
    if (p4Line.length() > 0) {
      Serial.print("RX <- P4: ");
      Serial.println(p4Line);
    }
  }

  delay(5);
}
