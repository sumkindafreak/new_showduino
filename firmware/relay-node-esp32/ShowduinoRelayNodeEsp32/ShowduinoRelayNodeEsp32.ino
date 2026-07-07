/*
  Showduino ESP32 Relay Node

  Role:
  - Receives relay commands over ESP-NOW.
  - Switches local relay module outputs.
  - Replies with ACK/status packets.

  Upload target:
  - ESP32 relay module board or ESP32 wired to relay module.

  Supported commands:
    RELAY:1:ON
    RELAY:1:OFF
    RELAY:1:PULSE:1000
    RELAY:ALL:OFF
    STATUS:REQUEST
    EMERGENCY:STOP
    EMERGENCY:CLEAR

  Setup:
  1. Upload this sketch to the relay node.
  2. Open Serial Monitor at 115200.
  3. Copy the printed MAC address.
  4. Paste it into RELAY_NODE_MAC in the ESP-NOW Bridge sketch.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define DEBUG_BAUD 115200
#define RELAY_COUNT 8
#define ESPNOW_CHANNEL 1
#define MAX_COMMAND_LENGTH 96

// Starter relay pins. Change these to match your ESP32 relay module board.
const uint8_t RELAY_PINS[RELAY_COUNT] = { 23, 22, 21, 19, 18, 5, 17, 16 };

// Most relay modules are active LOW. Set to false if your board uses active HIGH relays.
#define RELAY_ACTIVE_LOW true

// Optional status LED. Change if needed, or set to -1 if unused.
#define STATUS_LED_PIN 2

bool relays[RELAY_COUNT] = { false, false, false, false, false, false, false, false };
bool emergencyLocked = false;

uint8_t lastBridgeMac[6] = { 0, 0, 0, 0, 0, 0 };
bool hasBridgeMac = false;

struct ShowduinoEspNowPacket {
  char nodeType[16];
  char command[96];
  uint32_t sequence;
};

void setRelay(uint8_t channel, bool on);
void allRelaysOff();
void handleCommand(String command);

void copyMac(const uint8_t *source, uint8_t *dest) {
  for (uint8_t i = 0; i < 6; i++) {
    dest[i] = source[i];
  }
}

void sendReply(const String &message) {
  if (!hasBridgeMac) {
    Serial.print("No bridge MAC yet, local reply only: ");
    Serial.println(message);
    return;
  }

  ShowduinoEspNowPacket packet = {};
  strncpy(packet.nodeType, "RELAY", sizeof(packet.nodeType) - 1);
  strncpy(packet.command, message.c_str(), sizeof(packet.command) - 1);
  packet.sequence = millis();

  esp_err_t result = esp_now_send(lastBridgeMac, (uint8_t *)&packet, sizeof(packet));

  Serial.print("TX -> Bridge: ");
  Serial.print(message);
  Serial.print(" result=");
  Serial.println(result);
}

void addBridgePeerIfNeeded(const uint8_t *macAddress) {
  if (esp_now_is_peer_exist(macAddress)) {
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.print("Failed to add bridge peer. Error: ");
    Serial.println(result);
  }
}

void onEspNowReceived(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(ShowduinoEspNowPacket)) {
    Serial.println("Bad ESP-NOW packet size.");
    return;
  }

  copyMac(info->src_addr, lastBridgeMac);
  hasBridgeMac = true;
  addBridgePeerIfNeeded(info->src_addr);

  ShowduinoEspNowPacket packet;
  memcpy(&packet, incomingData, sizeof(packet));

  String command = String(packet.command);
  command.trim();

  Serial.print("RX <- Bridge: ");
  Serial.println(command);

  handleCommand(command);
}

void onEspNowSent(const uint8_t *macAddress, esp_now_send_status_t status) {
  Serial.print("ESP-NOW reply send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAILED");
}

void setRelay(uint8_t channel, bool on) {
  if (channel < 1 || channel > RELAY_COUNT) {
    sendReply("ERR:RELAY_NODE:INVALID_CHANNEL");
    return;
  }

  uint8_t index = channel - 1;
  relays[index] = on;

  bool outputLevel = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(RELAY_PINS[index], outputLevel ? HIGH : LOW);

  sendReply(String("OK:RELAY:") + channel + (on ? ":ON" : ":OFF"));
}

void allRelaysOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    relays[i] = false;
    bool outputLevel = RELAY_ACTIVE_LOW ? true : false;
    digitalWrite(RELAY_PINS[i], outputLevel ? HIGH : LOW);
  }
  sendReply("OK:RELAY:ALL:OFF");
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

void pulseRelay(uint8_t channel, unsigned long pulseMs) {
  if (pulseMs == 0 || pulseMs > 60000UL) {
    sendReply("ERR:RELAY_NODE:INVALID_PULSE_TIME");
    return;
  }

  setRelay(channel, true);
  delay(pulseMs);
  setRelay(channel, false);
  sendReply(String("OK:RELAY:") + channel + ":PULSE:" + pulseMs);
}

void sendStatus() {
  sendReply(emergencyLocked ? "STATUS:RELAY_NODE:EMERGENCY_LOCKED" : "STATUS:RELAY_NODE:READY");

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    sendReply(String("RELAY:") + (i + 1) + (relays[i] ? ":ON" : ":OFF"));
  }
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
      sendReply("ERR:RELAY_NODE:EMERGENCY_LOCKED");
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
      sendReply("ERR:RELAY_NODE:EMERGENCY_LOCKED");
      return;
    }
    unsigned long pulseMs = getToken(command, 3).toInt();
    pulseRelay(channel, pulseMs);
    return;
  }

  sendReply("ERR:RELAY_NODE:UNKNOWN_RELAY_ACTION");
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command == "STATUS:REQUEST") {
    sendStatus();
    return;
  }

  if (command == "EMERGENCY:STOP") {
    emergencyLocked = true;
    allRelaysOff();
    if (STATUS_LED_PIN >= 0) digitalWrite(STATUS_LED_PIN, HIGH);
    sendReply("STATUS:RELAY_NODE:EMERGENCY_LOCKED");
    return;
  }

  if (command == "EMERGENCY:CLEAR") {
    emergencyLocked = false;
    if (STATUS_LED_PIN >= 0) digitalWrite(STATUS_LED_PIN, LOW);
    sendReply("STATUS:RELAY_NODE:EMERGENCY_CLEARED");
    return;
  }

  if (command.startsWith("RELAY:")) {
    handleRelayCommand(command);
    return;
  }

  sendReply("ERR:RELAY_NODE:UNKNOWN_COMMAND");
}

void setupRelays() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    bool outputLevel = RELAY_ACTIVE_LOW ? true : false;
    digitalWrite(RELAY_PINS[i], outputLevel ? HIGH : LOW);
  }
}

void printMacAddress() {
  Serial.print("Relay Node MAC address: ");
  Serial.println(WiFi.macAddress());
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  printMacAddress();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed.");
    return;
  }

  esp_now_register_recv_cb(onEspNowReceived);
  esp_now_register_send_cb(onEspNowSent);

  Serial.println("ESP-NOW relay node ready.");
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino ESP32 Relay Node starting...");

  if (STATUS_LED_PIN >= 0) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
  }

  setupRelays();
  setupEspNow();
}

void loop() {
  if (emergencyLocked && STATUS_LED_PIN >= 0) {
    digitalWrite(STATUS_LED_PIN, (millis() / 250) % 2 == 0 ? HIGH : LOW);
  }
}
