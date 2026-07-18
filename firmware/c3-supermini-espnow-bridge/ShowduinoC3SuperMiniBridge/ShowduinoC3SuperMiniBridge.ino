/*
  Showduino ESP-NOW Bridge — ESP32-C3 SuperMini

  Roles:
  1) Director <-> P4 Stage Engine (desk link)
  2) P4 <-> Relay Node (node fabric)

  Path:
      Director --ESP-NOW(desk)--> C3 --UART--> P4
      P4 --UART ROUTE:RELAY:<cmd>--> C3 --ESP-NOW(node)--> Relay ESP32
      Relay --ESP-NOW--> C3 --UART NODE:<reply>--> P4 --UART--> C3 --ESP-NOW--> Director

  Arduino IDE:
    Board: ESP32C3 Dev Module
    USB CDC On Boot: Enabled
    Serial Monitor: 115200

  Wiring to P4:
    C3 TX GPIO21  ->  P4 RX GPIO5
    C3 RX GPIO20  <-  P4 TX GPIO6
    GND           --  GND

  Setup:
    1. Flash relay node, copy its MAC into RELAY_NODE_MAC_* below.
    2. Flash this C3 bridge.
    3. Copy C3 MAC into Director BoardConfig.h.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include "../../../protocol/showduino_desk_packet.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_validation.h"
#include "../../../protocol/showduino_state_wire.h"

#define USB_DEBUG_BAUD 115200
#define P4_UART_BAUD   115200
#define P4_UART_RX_PIN 20
#define P4_UART_TX_PIN 21

#define SHOWDUINO_ESPNOW_CHANNEL 1
/* Magic / wire version / command max: protocol/showduino_protocol_version.h */

// =========================================================
// Relay node MAC — paste from relay node Serial Monitor boot line
// Example: AA:BB:CC:DD:EE:FF
// =========================================================
#define RELAY_NODE_MAC_0 0xFF
#define RELAY_NODE_MAC_1 0xFF
#define RELAY_NODE_MAC_2 0xFF
#define RELAY_NODE_MAC_3 0xFF
#define RELAY_NODE_MAC_4 0xFF
#define RELAY_NODE_MAC_5 0xFF

/* Desk + node packets: protocol/showduino_{desk,node}_packet.h */

uint32_t receivedFromDirector = 0;
uint32_t rejectedPackets = 0;
uint32_t forwardedToDirector = 0;
uint32_t routedToRelay = 0;
uint16_t deskReplySequence = 1;
uint32_t nodeSequence = 1;

uint8_t bridgeMac[6] = {0};
uint8_t directorMac[6] = {0};
uint8_t pendingDirectorMac[6] = {0};
uint8_t relayNodeMac[6] = {
  RELAY_NODE_MAC_0, RELAY_NODE_MAC_1, RELAY_NODE_MAC_2,
  RELAY_NODE_MAC_3, RELAY_NODE_MAC_4, RELAY_NODE_MAC_5
};

bool directorKnown = false;
bool relayPeerReady = false;
volatile bool pendingDirector = false;
String usbBuffer;
String p4Buffer;

void printMac(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
  }
}

bool macIsZero(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0) return false;
  }
  return true;
}

bool macIsBroadcast(const uint8_t *mac) {
  for (uint8_t i = 0; i < 6; i++) {
    if (mac[i] != 0xFF) return false;
  }
  return true;
}

bool sameMac(const uint8_t *a, const uint8_t *b) {
  for (uint8_t i = 0; i < 6; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

bool readStaMac(uint8_t *outMac) {
  if (esp_read_mac(outMac, ESP_MAC_WIFI_STA) == ESP_OK && !macIsZero(outMac)) return true;
  String s = WiFi.macAddress();
  int values[6] = {0};
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; i++) outMac[i] = (uint8_t)values[i];
    return !macIsZero(outMac);
  }
  return false;
}

bool ensureDirectorPeer(const uint8_t *mac) {
  if (mac == nullptr || macIsZero(mac)) return false;
  if (directorKnown && sameMac(directorMac, mac) && esp_now_is_peer_exist(mac)) return true;

  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = SHOWDUINO_ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
      Serial.printf("ESP-NOW: failed to add Director peer (%d)\n", (int)err);
      return false;
    }
  }

  memcpy(directorMac, mac, 6);
  if (!directorKnown) {
    directorKnown = true;
    Serial.print("ESP-NOW: Director peer learned = ");
    printMac(directorMac);
    Serial.println();
  }
  return true;
}

bool ensureRelayPeer() {
  if (macIsBroadcast(relayNodeMac) || macIsZero(relayNodeMac)) {
    relayPeerReady = false;
    return false;
  }
  if (esp_now_is_peer_exist(relayNodeMac)) {
    relayPeerReady = true;
    return true;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, relayNodeMac, 6);
  peerInfo.channel = SHOWDUINO_ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  esp_err_t err = esp_now_add_peer(&peerInfo);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("ESP-NOW: failed to add Relay peer (%d)\n", (int)err);
    relayPeerReady = false;
    return false;
  }
  relayPeerReady = true;
  Serial.print("ESP-NOW: Relay peer = ");
  printMac(relayNodeMac);
  Serial.println();
  return true;
}

void forwardToP4(const char *command) {
  Serial1.println(command);
  if (strcmp(command, SHOWDUINO_LEGACY_HEARTBEAT) != 0 &&
      strcmp(command, SHOWDUINO_LEGACY_HELLO) != 0) {
    Serial.print("TX -> P4: ");
    Serial.println(command);
  }
}

bool forwardToDirector(const String &line) {
  if (!directorKnown) return false;
  if (!esp_now_is_peer_exist(directorMac)) ensureDirectorPeer(directorMac);

  ShowduinoDeskPacket packet = {};
  packet.magic = SHOWDUINO_ESPNOW_MAGIC;
  packet.version = SHOWDUINO_ESPNOW_VERSION;
  packet.sequence = deskReplySequence++;
  packet.sentMillis = millis();
  line.substring(0, SHOWDUINO_ESPNOW_COMMAND_MAX - 1).toCharArray(packet.command, SHOWDUINO_ESPNOW_COMMAND_MAX);

  esp_err_t err = esp_now_send(directorMac, (uint8_t *)&packet, sizeof(packet));
  if (err != ESP_OK) return false;

  forwardedToDirector++;
  if (line != SHOWDUINO_LEGACY_ACK_HEARTBEAT && line != SHOWDUINO_LEGACY_READY &&
      line != SHOWDUINO_LEGACY_SHOWDUINO_STAGE && line != "BOOT:STAGE_ENGINE_READY") {
    Serial.print("TX -> Director: ");
    Serial.println(packet.command);
  }
  return true;
}

bool routeToRelayNode(const String &command) {
  if (!ensureRelayPeer()) {
    forwardToP4("ERR:RELAY_NODE_MAC_NOT_SET");
    Serial.println("Set RELAY_NODE_MAC_* in C3 bridge sketch to the relay node MAC.");
    return false;
  }

  ShowduinoNodePacket packet = {};
  strncpy(packet.nodeType, SHOWDUINO_LEGACY_NODETYPE_RELAY, sizeof(packet.nodeType) - 1);
  command.substring(0, sizeof(packet.command) - 1).toCharArray(packet.command, sizeof(packet.command));
  packet.sequence = nodeSequence++;

  esp_err_t err = esp_now_send(relayNodeMac, (uint8_t *)&packet, sizeof(packet));
  if (err != ESP_OK) {
    forwardToP4("ERR:RELAY_NODE_SEND_FAILED");
    Serial.printf("ESP-NOW: relay send failed (%d)\n", (int)err);
    return false;
  }

  routedToRelay++;
  Serial.print("TX -> RelayNode: ");
  Serial.println(command);
  return true;
}

void handleP4Line(String line) {
  line.trim();
  if (line.length() == 0) return;

  // P4 asks C3 to deliver a command to the relay node.
  // Format: ROUTE:RELAY:<command...>
  if (line.startsWith(SHOWDUINO_LEGACY_ROUTE_RELAY)) {
    String cmd = line.substring(strlen(SHOWDUINO_LEGACY_ROUTE_RELAY));
    cmd.trim();
    if (line != SHOWDUINO_LEGACY_ACK_HEARTBEAT) {
      Serial.print("RX <- P4 ROUTE: ");
      Serial.println(cmd);
    }
    routeToRelayNode(cmd);
    return;
  }

  // Pixel/audio nodes are not implemented — do not report false success.
  if (line.startsWith(SHOWDUINO_LEGACY_ROUTE_PIXEL)) {
    Serial.print("RX <- P4 (pixel unavailable): ");
    Serial.println(line);
    forwardToDirector(String(SHOWDUINO_WIRE_NODE_UNAVAILABLE_PREFIX) + "PIXEL");
    return;
  }
  if (line.startsWith(SHOWDUINO_LEGACY_ROUTE_AUDIO)) {
    Serial.print("RX <- P4 (audio unavailable): ");
    Serial.println(line);
    forwardToDirector(String(SHOWDUINO_WIRE_NODE_UNAVAILABLE_PREFIX) + "AUDIO");
    return;
  }

  if (line != SHOWDUINO_LEGACY_ACK_HEARTBEAT &&
      line != SHOWDUINO_LEGACY_READY &&
      line != SHOWDUINO_LEGACY_SHOWDUINO_STAGE &&
      line != "BOOT:STAGE_ENGINE_READY") {
    Serial.print("RX <- P4: ");
    Serial.println(line);
  }
  forwardToDirector(line);
}

void handleRelayNodeReply(const ShowduinoNodePacket &packet, const uint8_t *srcMac) {
  // Learn/refresh peer from replies (in case MAC config was wrong / changed).
  if (srcMac != nullptr && !macIsZero(srcMac)) {
    if (!sameMac(relayNodeMac, srcMac) && !macIsBroadcast(relayNodeMac)) {
      // Keep configured MAC; still accept replies.
    }
    if (!esp_now_is_peer_exist(srcMac)) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, srcMac, 6);
      peerInfo.channel = SHOWDUINO_ESPNOW_CHANNEL;
      peerInfo.encrypt = false;
      peerInfo.ifidx = WIFI_IF_STA;
      esp_now_add_peer(&peerInfo);
    }
  }

  String reply = String(packet.command);
  reply.trim();
  Serial.print("RX <- RelayNode: ");
  Serial.println(reply);

  // Deliver raw node reply to P4 for translation / show state.
  forwardToP4((String(SHOWDUINO_LEGACY_NODE_PREFIX) + reply).c_str());
}

void onEspNowReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
  // Relay / node replies (no magic on v1 node packet — size + field validation)
  if (len == (int)sizeof(ShowduinoNodePacket)) {
    ShowduinoValidateResult vr = showduino_validate_node_rx(incomingData, (size_t)len);
    if (vr != SHOWDUINO_VALID) {
      rejectedPackets++;
      Serial.printf("ESP-NOW: node packet rejected (%d)\n", (int)vr);
      return;
    }
    ShowduinoNodePacket packet = {};
    memcpy(&packet, incomingData, sizeof(packet));
    handleRelayNodeReply(packet, recvInfo ? recvInfo->src_addr : nullptr);
    return;
  }

  // Director desk packets
  ShowduinoValidateResult deskVr = showduino_validate_desk_rx(incomingData, (size_t)len);
  if (deskVr != SHOWDUINO_VALID) {
    rejectedPackets++;
    Serial.printf("ESP-NOW: desk packet rejected (%d)\n", (int)deskVr);
    return;
  }

  ShowduinoDeskPacket packet = {};
  memcpy(&packet, incomingData, sizeof(packet));
  receivedFromDirector++;

  if (recvInfo != nullptr && !macIsZero(recvInfo->src_addr)) {
    memcpy(pendingDirectorMac, recvInfo->src_addr, 6);
    pendingDirector = true;
  }

  if (strcmp(packet.command, SHOWDUINO_LEGACY_HEARTBEAT) != 0 &&
      strcmp(packet.command, SHOWDUINO_LEGACY_HELLO) != 0) {
    Serial.print("RX <- Director ");
    if (recvInfo != nullptr) printMac(recvInfo->src_addr);
    Serial.printf(" cmd=%s\n", packet.command);
  }

  forwardToP4(packet.command);
}

void handleUsbLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "HELP") {
    Serial.println("Commands: HELP, STATUS, MAC, TEST:HELLO, TEST:RELAY:1:ON, or any P4 command");
    return;
  }

  if (line == "MAC" || line == "STATUS") {
    Serial.println("--- C3 SuperMini Bridge ---");
    Serial.print("Bridge MAC: "); printMac(bridgeMac); Serial.println();
    Serial.print("Director: ");
    if (directorKnown) printMac(directorMac); else Serial.print("(none)");
    Serial.println();
    Serial.print("Relay node: "); printMac(relayNodeMac);
    Serial.println(relayPeerReady ? " (peer ok)" : " (set MAC!)");
    Serial.printf("Director RX: %lu  Relay TX: %lu  Director TX: %lu\n",
                  (unsigned long)receivedFromDirector,
                  (unsigned long)routedToRelay,
                  (unsigned long)forwardedToDirector);
    return;
  }

  if (line == "TEST:HELLO") {
    forwardToP4("HELLO");
    return;
  }

  if (line.startsWith("TEST:RELAY:")) {
    routeToRelayNode(line.substring(5));  // strip "TEST:"
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
      if (usbBuffer.length() > 180) usbBuffer = "";
    }
  }
}

void readP4Serial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (p4Buffer.length() > 0) {
        handleP4Line(p4Buffer);
        p4Buffer = "";
      }
    } else {
      p4Buffer += c;
      if (p4Buffer.length() > 180) p4Buffer = "";
    }
  }
}

bool beginWifiSta() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(200);

  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
    WiFi.mode(WIFI_STA);
    delay(100);
    esp_wifi_start();
  }

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(SHOWDUINO_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);
  return readStaMac(bridgeMac);
}

void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Showduino C3 Bridge (Desk + Relay Node)");
  Serial.println("========================================");

  Serial1.begin(P4_UART_BAUD, SERIAL_8N1, P4_UART_RX_PIN, P4_UART_TX_PIN);
  Serial.printf("P4 UART: RX=%d TX=%d baud=%d\n", P4_UART_RX_PIN, P4_UART_TX_PIN, P4_UART_BAUD);

  if (!beginWifiSta()) {
    Serial.println("ERROR: Wi-Fi STA MAC is 00:00:00:00:00:00");
  } else {
    Serial.print("Bridge MAC: ");
    printMac(bridgeMac);
    Serial.println();
    Serial.println("Copy into Director BoardConfig.h (SHOWDUINO_P4_C6_MAC_*)");
  }

  Serial.print("Relay node MAC config: ");
  printMac(relayNodeMac);
  Serial.println();
  if (macIsBroadcast(relayNodeMac)) {
    Serial.println("WARNING: set RELAY_NODE_MAC_* before relay routing will work");
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: init failed — halting");
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(onEspNowReceive);
  ensureRelayPeer();
  Serial.println("ESP-NOW: desk + relay bridge ready");
  Serial.println("Type HELP / STATUS");
}

void loop() {
  if (macIsZero(bridgeMac)) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 2000) {
      lastTry = millis();
      if (beginWifiSta()) {
        Serial.print("Recovered Bridge MAC: ");
        printMac(bridgeMac);
        Serial.println();
      }
    }
  }

  readUsbSerial();
  if (pendingDirector) {
    pendingDirector = false;
    ensureDirectorPeer(pendingDirectorMac);
  }
  readP4Serial();
  delay(5);
}
