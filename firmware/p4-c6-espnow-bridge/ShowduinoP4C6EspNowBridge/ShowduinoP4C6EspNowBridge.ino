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
  - UART pins match the P4 Stage Engine BoardConfig (Serial1 RX=4 TX=5, 115200 8N1).
  - Wire crossed: C6 GPIO4 RX <- P4 GPIO5 TX, C6 GPIO5 TX -> P4 GPIO4 RX.
  - Matching GPIO numbers are separate nets, not factory-internal wiring.
  - Factory P4↔C6 SDIO exists on the module and is NOT used by this sketch.
  - Do not install ESP-Hosted. This sketch is the Showduino ESP-NOW UART bridge.
  - The C6 UART0 header is for flashing/debug, not this P4 link.
  - The sketch prints the C6 MAC address at boot. Copy that MAC into the Director's BoardConfig.h.
*/

#include <Arduino.h>
#include <string.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#ifdef ESP_ARDUINO_VERSION_STR
#include <esp_arduino_version.h>
#endif

// =========================================================
// Pin and serial configuration
// =========================================================
#define USB_DEBUG_BAUD 115200
#define P4_UART_BAUD   115200

// Must match firmware/stage-engine-p4 BoardConfig.h SHOWDUINO_C6_UART_* (crossed).
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
uint8_t directorMac[6] = {0};
bool haveDirector = false;
uint16_t txSequence = 1;
String p4InputBuffer;

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

bool readStaMac(uint8_t *outMac) {
  if (esp_read_mac(outMac, ESP_MAC_WIFI_STA) == ESP_OK) {
    bool zero = true;
    for (int i = 0; i < 6; i++) if (outMac[i] != 0) zero = false;
    if (!zero) return true;
  }
  return false;
}

// =========================================================
// Forward command to the P4 host
// =========================================================
void forwardToP4(const char *command) {
  Serial1.println(command);
  Serial.print("TX -> P4: ");
  Serial.println(command);
}

bool addDirectorPeer(const uint8_t *mac) {
  if (!mac) return false;
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = SHOWDUINO_ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_err_t err = esp_now_add_peer(&peer);
  return err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST;
}

void forwardToDirector(const char *line) {
  if (!haveDirector || !line || !line[0]) return;
  size_t n = strlen(line);
  if (n >= SHOWDUINO_ESPNOW_COMMAND_MAX) {
    Serial.println("P4 line too long for ESP-NOW desk packet; dropped.");
    return;
  }

  ShowduinoEspNowPacket packet = {};
  packet.magic = SHOWDUINO_ESPNOW_MAGIC;
  packet.version = SHOWDUINO_ESPNOW_VERSION;
  packet.sequence = txSequence++;
  packet.sentMillis = millis();
  memcpy(packet.command, line, n);
  packet.command[n] = '\0';

  if (!addDirectorPeer(directorMac)) return;
  (void)esp_now_send(directorMac, (uint8_t *)&packet, sizeof(packet));
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
  if (recvInfo && recvInfo->src_addr) {
    memcpy(directorMac, recvInfo->src_addr, 6);
    haveDirector = true;
    addDirectorPeer(directorMac);
  }

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
  Serial.println("[C6] Showduino onboard C6 bridge");
#ifdef ESP_ARDUINO_VERSION_STR
  Serial.printf("[C6] Firmware: Arduino %s / IDF %s\n",
                ESP_ARDUINO_VERSION_STR, ESP.getSdkVersion());
#else
  Serial.printf("[C6] Firmware: IDF %s built %s %s\n",
                ESP.getSdkVersion(), __DATE__, __TIME__);
#endif
  Serial.printf("[C6] UART RX=%d TX=%d baud=%d\n",
                P4_UART_RX_PIN, P4_UART_TX_PIN, P4_UART_BAUD);
  Serial.println("[C6] Wiring (crossed): C6 GPIO5 TX -> P4 GPIO4 RX, P4 GPIO5 TX -> C6 GPIO4 RX");

  Serial1.setRxBufferSize(512);
  Serial1.begin(P4_UART_BAUD, SERIAL_8N1, P4_UART_RX_PIN, P4_UART_TX_PIN);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(200);
  esp_wifi_start();
  esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  uint8_t mac[6] = {0};
  Serial.print("[C6] MAC=");
  if (readStaMac(mac)) {
    printMac(mac);
    Serial.println();
  } else {
    Serial.println(WiFi.macAddress());
  }
  if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
      mac[3] == 0 && mac[4] == 0 && mac[5] == 0) {
    Serial.println("[C6] WARNING: MAC is all zeros — Wi-Fi radio not ready. ESP-NOW will fail.");
  } else {
    Serial.println("[C6] Copy this MAC into the Director BoardConfig.h peer fields.");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[C6] ESP-NOW: init failed. Halting bridge.");
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(onEspNowReceive);
  Serial.println("[C6] ESP-NOW ready");
  Serial.println("[C6] Waiting for P4 UART");
  Serial.println("[C6] Type HELP for USB debug commands.");
}

// =========================================================
// Main loop
// =========================================================
void loop() {
  readUsbSerial();

  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
        if (p4InputBuffer.length() > 0) {
          p4InputBuffer.trim();
          if (p4InputBuffer.length() > 0) {
            if (p4InputBuffer == "DIAG:PING") {
              Serial.println("[C6] UART RX: DIAG:PING");
              Serial1.println("DIAG:PONG");
              Serial1.flush();
              Serial.println("[C6] UART TX: DIAG:PONG");
            } else {
              forwardToDirector(p4InputBuffer.c_str());
            }
          }
        p4InputBuffer = "";
      }
    } else {
      p4InputBuffer += c;
      if (p4InputBuffer.length() > 180) {
        p4InputBuffer = "";
        Serial.println("P4 line too long; buffer cleared.");
      }
    }
  }

  delay(5);
}
