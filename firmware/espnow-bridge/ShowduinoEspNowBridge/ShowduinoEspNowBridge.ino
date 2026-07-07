/*
  Showduino ESP-NOW Bridge

  Role:
  - Receives text commands from the ESP32-P4 Stage Engine over UART.
  - Sends relay commands wirelessly to ESP32 Relay Nodes using ESP-NOW.
  - Forwards basic status/ACK replies back to the Stage Engine.

  Upload target:
  - ESP32-C3 / ESP32-C6 / ESP32 / ESP32-S3 bridge board.
  - On the Waveshare ESP32-P4 board, this role can later move to the onboard ESP32-C6.

  Starter commands from Stage Engine:
    BRIDGE:HELLO
    RELAY:1:ON
    RELAY:1:OFF
    RELAY:1:PULSE:1000
    RELAY:ALL:OFF
    EMERGENCY:STOP
    EMERGENCY:CLEAR

  Important:
  - Update RELAY_NODE_MAC below with your relay node MAC address.
  - Relay node prints its MAC address on boot.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define DEBUG_BAUD 115200
#define STAGE_BAUD 115200

// UART pins from P4 Stage Engine to this bridge.
// Change these when the final bridge wiring is chosen.
#define STAGE_RX_PIN 18
#define STAGE_TX_PIN 19

#define ESPNOW_CHANNEL 1
#define MAX_COMMAND_LENGTH 160

// Replace this with the relay node MAC printed by the relay node Serial Monitor.
uint8_t RELAY_NODE_MAC[] = { 0x24, 0x6F, 0x28, 0x00, 0x00, 0x00 };

String stageInputBuffer = "";
bool lastSendOk = false;

struct ShowduinoEspNowPacket {
  char nodeType[16];
  char command[96];
  uint32_t sequence;
};

uint32_t packetSequence = 0;

void sendToStage(const String &message) {
  Serial1.println(message);
  Serial.print("TX -> Stage: ");
  Serial.println(message);
}

void onEspNowSent(const uint8_t *macAddress, esp_now_send_status_t status) {
  lastSendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendToStage(lastSendOk ? "OK:ESPNOW:SENT" : "ERR:ESPNOW:SEND_FAILED");
}

void onEspNowReceived(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(ShowduinoEspNowPacket)) {
    sendToStage("ERR:ESPNOW:BAD_PACKET_SIZE");
    return;
  }

  ShowduinoEspNowPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));

  Serial.print("ESP-NOW RX from node: ");
  Serial.println(packet.command);

  sendToStage(String("NODE:") + packet.command);
}

bool addRelayPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, RELAY_NODE_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_is_peer_exist(RELAY_NODE_MAC)) {
    return true;
  }

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.print("Failed to add ESP-NOW peer. Error: ");
    Serial.println(result);
    return false;
  }

  return true;
}

bool sendEspNowCommand(const String &command) {
  if (!addRelayPeer()) {
    sendToStage("ERR:ESPNOW:PEER_ADD_FAILED");
    return false;
  }

  ShowduinoEspNowPacket packet = {};
  strncpy(packet.nodeType, "RELAY", sizeof(packet.nodeType) - 1);
  strncpy(packet.command, command.c_str(), sizeof(packet.command) - 1);
  packet.sequence = ++packetSequence;

  esp_err_t result = esp_now_send(RELAY_NODE_MAC, (uint8_t *)&packet, sizeof(packet));

  if (result != ESP_OK) {
    sendToStage("ERR:ESPNOW:SEND_START_FAILED");
    return false;
  }

  sendToStage(String("OK:ESPNOW:QUEUED:") + command);
  return true;
}

void handleStageCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  Serial.print("RX <- Stage: ");
  Serial.println(command);

  if (command == "BRIDGE:HELLO" || command == "HELLO") {
    sendToStage("SHOWDUINO_ESPNOW_BRIDGE");
    sendToStage("FW:0.1.0");
    sendToStage("MODE:RELAY_ROUTE");
    sendToStage("READY");
    return;
  }

  if (command == "STATUS:REQUEST") {
    sendToStage("STATUS:BRIDGE_READY");
    return;
  }

  if (command.startsWith("RELAY:") || command.startsWith("EMERGENCY:")) {
    sendEspNowCommand(command);
    return;
  }

  sendToStage("ERR:BRIDGE:UNKNOWN_COMMAND");
}

void readStageSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\n' || c == '\r') {
      if (stageInputBuffer.length() > 0) {
        handleStageCommand(stageInputBuffer);
        stageInputBuffer = "";
      }
    } else {
      stageInputBuffer += c;
      if (stageInputBuffer.length() > MAX_COMMAND_LENGTH) {
        stageInputBuffer = "";
        sendToStage("ERR:BRIDGE:COMMAND_TOO_LONG");
      }
    }
  }
}

void printMacAddress() {
  Serial.print("Bridge MAC address: ");
  Serial.println(WiFi.macAddress());
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  printMacAddress();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed.");
    sendToStage("ERR:ESPNOW:INIT_FAILED");
    return;
  }

  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowReceived);

  if (addRelayPeer()) {
    Serial.println("Relay node peer added.");
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino ESP-NOW Bridge starting...");

  Serial1.begin(STAGE_BAUD, SERIAL_8N1, STAGE_RX_PIN, STAGE_TX_PIN);

  setupEspNow();
  sendToStage("BOOT:ESPNOW_BRIDGE_READY");
}

void loop() {
  readStageSerial();
}
