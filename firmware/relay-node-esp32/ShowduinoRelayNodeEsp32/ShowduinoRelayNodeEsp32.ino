/*
  Showduino ESP32 Relay Node

  Role:
  - Receives relay commands over ESP-NOW from the C3 bridge.
  - Switches local relay module outputs.
  - Replies with OK/ACK/status packets to the bridge.

  Path:
      Director -> C3 -> P4 -> C3 -> this Relay Node
      this Relay Node -> C3 -> P4 -> C3 -> Director

  Supported commands:
    RELAY:1:ON
    RELAY:1:OFF
    RELAY:1:TOGGLE
    RELAY:1:PULSE:1000
    RELAY:ALL:OFF
    STATUS:REQUEST
    EMERGENCY:STOP
    EMERGENCY:CLEAR

  Setup:
  1. Upload this sketch to the relay ESP32.
  2. Open Serial Monitor at 115200.
  3. Copy the printed MAC into C3 bridge RELAY_NODE_MAC_* defines.
  4. Flash C3 bridge, P4 Stage Engine, Director.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_validation.h"

#define DEBUG_BAUD 115200
#define RELAY_COUNT 8
#define ESPNOW_CHANNEL 1

// Change these to match your ESP32 relay module board.
const uint8_t RELAY_PINS[RELAY_COUNT] = { 23, 22, 21, 19, 18, 5, 17, 16 };
#define RELAY_ACTIVE_LOW true
#define STATUS_LED_PIN 2

bool relays[RELAY_COUNT] = {};
bool emergencyLocked = false;

uint8_t lastBridgeMac[6] = {0};
bool hasBridgeMac = false;

/* Node packet: protocol/showduino_node_packet.h */

void setRelay(uint8_t channel, bool on);
void allRelaysOff();
void handleCommand(String command);

void copyMac(const uint8_t *source, uint8_t *dest) {
  for (uint8_t i = 0; i < 6; i++) dest[i] = source[i];
}

bool macIsZero(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0) return false;
  }
  return true;
}

void sendReply(const String &message) {
  if (!hasBridgeMac || macIsZero(lastBridgeMac)) {
    Serial.print("No bridge MAC yet, local reply only: ");
    Serial.println(message);
    return;
  }

  ShowduinoNodePacket packet = {};
  strncpy(packet.nodeType, SHOWDUINO_LEGACY_NODETYPE_RELAY, sizeof(packet.nodeType) - 1);
  message.substring(0, sizeof(packet.command) - 1).toCharArray(packet.command, sizeof(packet.command));
  packet.sequence = millis();

  esp_err_t result = esp_now_send(lastBridgeMac, (uint8_t *)&packet, sizeof(packet));
  Serial.print("TX -> Bridge: ");
  Serial.print(message);
  Serial.print(" result=");
  Serial.println((int)result);
}

void addBridgePeerIfNeeded(const uint8_t *macAddress) {
  if (macAddress == nullptr || macIsZero(macAddress)) return;
  if (esp_now_is_peer_exist(macAddress)) return;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = 0; /* current home channel — SoftAP-safe */
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("Failed to add bridge peer. Error: ");
    Serial.println((int)result);
  }
}

void onEspNowReceived(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  ShowduinoValidateResult vr = showduino_validate_node_rx(incomingData, (size_t)len);
  if (vr != SHOWDUINO_VALID) {
    Serial.printf("Bad ESP-NOW node packet (%d) len=%d\n", (int)vr, len);
    return;
  }

  if (info != nullptr) {
    copyMac(info->src_addr, lastBridgeMac);
    hasBridgeMac = true;
    addBridgePeerIfNeeded(info->src_addr);
  }

  ShowduinoNodePacket packet = {};
  memcpy(&packet, incomingData, sizeof(packet));

  String command = String(packet.command);
  command.trim();

  Serial.print("RX <- Bridge: ");
  Serial.println(command);
  handleCommand(command);
}

#if defined(ESP_IDF_VERSION) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
void onEspNowSent(const esp_now_send_info_t *txInfo, esp_now_send_status_t status) {
  (void)txInfo;
#else
void onEspNowSent(const uint8_t *macAddress, esp_now_send_status_t status) {
  (void)macAddress;
#endif
  Serial.print("ESP-NOW reply send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAILED");
}

void writeRelayPin(uint8_t index, bool on) {
  bool outputLevel = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(RELAY_PINS[index], outputLevel ? HIGH : LOW);
}

void setRelay(uint8_t channel, bool on) {
  if (channel < 1 || channel > RELAY_COUNT) {
    sendReply("ERR:RELAY_NODE:INVALID_CHANNEL");
    return;
  }

  uint8_t index = channel - 1;
  relays[index] = on;
  writeRelayPin(index, on);
  sendReply(String("OK:RELAY:") + channel + (on ? ":ON" : ":OFF"));
}

void allRelaysOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    relays[i] = false;
    writeRelayPin(i, false);
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

  uint8_t channel = (uint8_t)channelToken.toInt();

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

  if (action == "TOGGLE") {
    if (emergencyLocked) {
      sendReply("ERR:RELAY_NODE:EMERGENCY_LOCKED");
      return;
    }
    if (channel < 1 || channel > RELAY_COUNT) {
      sendReply("ERR:RELAY_NODE:INVALID_CHANNEL");
      return;
    }
    setRelay(channel, !relays[channel - 1]);
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
    writeRelayPin(i, false);
    relays[i] = false;
  }
}

void setupEspNow() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  Serial.print("Relay Node MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Copy this MAC into C3 bridge RELAY_NODE_MAC_*");

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
  delay(10);
}
