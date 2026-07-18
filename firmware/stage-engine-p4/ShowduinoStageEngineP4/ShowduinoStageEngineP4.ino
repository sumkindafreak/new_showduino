/*
  Showduino Show Engine - ESP32-P4 (Stage Controller product)

  Role: authoritative runtime state + command validation.
  Does NOT drive relays locally — routes to the Relay Node via C3.

  Path:
      Director --ESP-NOW--> C3 --UART--> P4
      P4 --UART ROUTE:RELAY:<cmd>--> C3 --ESP-NOW--> Relay ESP32
      Relay --ESP-NOW--> C3 --UART NODE:<reply>--> P4 --UART--> C3 --ESP-NOW--> Director

  Arduino IDE:
      Board: ESP32P4 Dev Module
      Flash Size: 16MB
      USB CDC On Boot: Enabled
*/

#include <Arduino.h>
#include "../../../protocol/showduino_protocol_version.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "ShowEngineState.h"

#define DEBUG_BAUD 115200
#define LINK_BAUD  115200
#define LINK_RX_PIN 5
#define LINK_TX_PIN 6
#define LINK_DEBUG 0
#define CMD_MAX_LEN SHOWDUINO_DESK_COMMAND_MAX

ShowEngineState gState;
String inputBuffer;
uint32_t noiseBytes = 0;
uint32_t goodCommands = 0;

bool isQuietLinkMessage(const String &message) {
  return message == SHOWDUINO_LEGACY_HEARTBEAT ||
         message == SHOWDUINO_LEGACY_ACK_HEARTBEAT ||
         message == SHOWDUINO_LEGACY_HELLO ||
         message == SHOWDUINO_LEGACY_READY ||
         message == SHOWDUINO_LEGACY_SHOWDUINO_STAGE ||
         message == "BOOT:STAGE_ENGINE_READY";
}

void sendToLink(const String &message) {
  Serial1.println(message);
#if LINK_DEBUG
  if (!isQuietLinkMessage(message)) {
    Serial.print("TX -> link: ");
    Serial.println(message);
  }
#endif
}

void flushLinkRx() {
  while (Serial1.available() > 0) {
    (void)Serial1.read();
  }
  inputBuffer = "";
}

bool looksLikeNoise(const String &s) {
  if (s.length() == 0) return true;
  uint8_t bad = 0;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    bool ok = (c >= 32 && c <= 126) || c == '\t';
    if (!ok) bad++;
  }
  return bad > 0;
}

void publishShowState() {
  sendToLink(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + showRuntimeWire(gState.show));
}

void publishEmergencyState() {
  sendToLink(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + emergencyWire(gState.emergency));
  /* Legacy companions for older Director builds */
  if (gState.emergency == EmergencyState::Active) {
    sendToLink(SHOWDUINO_LEGACY_STATUS_ELOCKED);
  } else {
    sendToLink(SHOWDUINO_LEGACY_STATUS_ECLEARED);
  }
}

void publishNodeState() {
  sendToLink(String(SHOWDUINO_WIRE_STATE_NODE_RELAY_PREFIX) + nodeAvailWire(gState.relayNode));
}

void publishRelayState(uint8_t channel) {
  if (channel < 1 || channel > SHOW_ENGINE_RELAY_COUNT) return;
  RelayKnowledge k = gState.relays[channel - 1].confirmed;
  sendToLink(String(SHOWDUINO_WIRE_STATE_RELAY_PREFIX) + channel + ":" + relayKnowledgeWire(k));
}

void publishAllRelayStates() {
  for (uint8_t i = 1; i <= SHOW_ENGINE_RELAY_COUNT; i++) {
    publishRelayState(i);
  }
}

void routeToRelayNode(const String &command) {
  gState.routedCommands++;
  sendToLink(String(SHOWDUINO_LEGACY_ROUTE_RELAY) + command);
}

void setConfirmedRelay(uint8_t channel, RelayKnowledge k) {
  if (channel < 1 || channel > SHOW_ENGINE_RELAY_COUNT) return;
  RelayChannelState &ch = gState.relays[channel - 1];
  ch.confirmed = k;
  ch.pending = false;
  showEngineBump(gState);
  publishRelayState(channel);
}

void clearPending(uint8_t channel) {
  if (channel < 1 || channel > SHOW_ENGINE_RELAY_COUNT) return;
  gState.relays[channel - 1].pending = false;
}

void failPending(uint8_t channel, const char *reason) {
  if (channel < 1 || channel > SHOW_ENGINE_RELAY_COUNT) return;
  clearPending(channel);
  sendToLink(String(SHOWDUINO_WIRE_FAILED_RELAY_PREFIX) + channel + ":" + reason);
  publishRelayState(channel); /* reaffirm last confirmed */
}

void rejectRelay(uint8_t channel, const char *reason) {
  sendToLink(String(SHOWDUINO_WIRE_REJECTED_RELAY_PREFIX) + channel + ":" + reason);
}

bool parseRelayAbsolute(const String &command, uint8_t *channelOut, bool *onOut) {
  if (!command.startsWith("RELAY:")) return false;
  int first = command.indexOf(':');
  int second = command.indexOf(':', first + 1);
  if (first < 0 || second < 0) return false;
  String chTok = command.substring(first + 1, second);
  String action = command.substring(second + 1);
  if (chTok == "ALL") return false;
  uint8_t ch = (uint8_t)chTok.toInt();
  if (ch < 1 || ch > SHOW_ENGINE_RELAY_COUNT) return false;
  if (action == "ON") {
    *channelOut = ch;
    *onOut = true;
    return true;
  }
  if (action == "OFF") {
    *channelOut = ch;
    *onOut = false;
    return true;
  }
  return false;
}

void acceptAndRouteRelay(uint8_t channel, bool on) {
  RelayChannelState &ch = gState.relays[channel - 1];
  uint16_t seq = gState.nextRequestSeq++;
  if (gState.nextRequestSeq == 0) gState.nextRequestSeq = 1;

  ch.pending = true;
  ch.pendingOn = on;
  ch.pendingSeq = seq;
  ch.pendingSinceMs = millis();

  sendToLink(String(SHOWDUINO_WIRE_ACCEPTED_RELAY_PREFIX) + seq + ":" + channel +
             (on ? ":ON" : ":OFF"));
  routeToRelayNode(String("RELAY:") + channel + (on ? ":ON" : ":OFF"));
}

void handleRelaySetRequest(const String &command) {
  uint8_t channel = 0;
  bool on = false;

  if (command == SHOWDUINO_LEGACY_RELAY_ALL_OFF) {
    if (gState.emergency == EmergencyState::Active) {
      rejectRelay(1, "EMERGENCY_ACTIVE");
      return;
    }
    if (gState.relayNode == NodeAvailability::Offline) {
      rejectRelay(1, "NODE_OFFLINE");
      return;
    }
    for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
      gState.relays[i].pending = true;
      gState.relays[i].pendingOn = false;
      gState.relays[i].pendingSeq = gState.nextRequestSeq;
      gState.relays[i].pendingSinceMs = millis();
    }
    uint16_t seq = gState.nextRequestSeq++;
    if (gState.nextRequestSeq == 0) gState.nextRequestSeq = 1;
    sendToLink(String(SHOWDUINO_WIRE_ACCEPTED_RELAY_PREFIX) + seq + ":ALL:OFF");
    routeToRelayNode(SHOWDUINO_LEGACY_RELAY_ALL_OFF);
    return;
  }

  if (command.indexOf(":TOGGLE") >= 0) {
    /* Deprecated: convert only when confirmed ON/OFF known */
    int first = command.indexOf(':');
    int second = command.indexOf(':', first + 1);
    uint8_t ch = (uint8_t)command.substring(first + 1, second).toInt();
    if (ch < 1 || ch > SHOW_ENGINE_RELAY_COUNT) {
      rejectRelay(ch ? ch : 1, "INVALID_CHANNEL");
      return;
    }
    RelayKnowledge k = gState.relays[ch - 1].confirmed;
    if (k != RelayKnowledge::On && k != RelayKnowledge::Off) {
      rejectRelay(ch, "STATE_UNKNOWN");
      return;
    }
    if (gState.relays[ch - 1].pending) {
      rejectRelay(ch, "BUSY");
      return;
    }
    if (gState.emergency == EmergencyState::Active) {
      rejectRelay(ch, "EMERGENCY_ACTIVE");
      return;
    }
    if (gState.relayNode == NodeAvailability::Offline) {
      rejectRelay(ch, "NODE_OFFLINE");
      return;
    }
    acceptAndRouteRelay(ch, k == RelayKnowledge::Off);
    return;
  }

  if (!parseRelayAbsolute(command, &channel, &on)) {
    if (command.indexOf(":PULSE:") >= 0) {
      /* Keep pulse for selftest; treat as accepted route without pending model */
      if (gState.emergency == EmergencyState::Active) {
        rejectRelay(1, "EMERGENCY_ACTIVE");
        return;
      }
      routeToRelayNode(command);
      return;
    }
    rejectRelay(1, "INVALID_CHANNEL");
    return;
  }

  if (gState.emergency == EmergencyState::Active && on) {
    rejectRelay(channel, "EMERGENCY_ACTIVE");
    return;
  }
  if (gState.relayNode == NodeAvailability::Offline) {
    rejectRelay(channel, "NODE_OFFLINE");
    return;
  }
  if (gState.relays[channel - 1].pending) {
    rejectRelay(channel, "BUSY");
    return;
  }

  acceptAndRouteRelay(channel, on);
}

void publishSnapshot() {
  sendToLink(SHOWDUINO_WIRE_SNAPSHOT_BEGIN);
  publishShowState();
  sendToLink(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + emergencyWire(gState.emergency));
  publishNodeState();
  publishAllRelayStates();
  sendToLink(SHOWDUINO_WIRE_SNAPSHOT_END);
}

void handleNodeReply(String reply) {
  reply.trim();
  if (reply.length() == 0) return;

  showEngineMarkRelayNodeSeen(gState, millis());

#if LINK_DEBUG
  Serial.print("NODE reply: ");
  Serial.println(reply);
#endif

  if (reply.startsWith(SHOWDUINO_LEGACY_OK_RELAY_PREFIX)) {
    /* OK:RELAY:1:ON or OK:RELAY:ALL:OFF or OK:RELAY:1:PULSE:ms */
    if (reply == "OK:RELAY:ALL:OFF") {
      for (uint8_t i = 1; i <= SHOW_ENGINE_RELAY_COUNT; i++) {
        setConfirmedRelay(i, RelayKnowledge::Off);
      }
      return;
    }
    if (reply.indexOf(":PULSE:") >= 0) {
      /* Intermediate pulse ACKs also arrive as OK:RELAY:n:ON/OFF via setRelay */
      return;
    }
    int numStart = 9; /* after OK:RELAY: */
    int numEnd = reply.indexOf(':', numStart);
    if (numEnd > numStart) {
      uint8_t ch = (uint8_t)reply.substring(numStart, numEnd).toInt();
      if (ch >= 1 && ch <= SHOW_ENGINE_RELAY_COUNT) {
        bool on = reply.endsWith(":ON");
        setConfirmedRelay(ch, on ? RelayKnowledge::On : RelayKnowledge::Off);
      }
    }
    return;
  }

  if (reply == "STATUS:RELAY_NODE:EMERGENCY_LOCKED") {
    /* Node local lock observed — Show Engine already owns emergency policy */
    showEngineMarkRelayNodeSeen(gState, millis());
    return;
  }
  if (reply == "STATUS:RELAY_NODE:EMERGENCY_CLEARED") {
    showEngineMarkRelayNodeSeen(gState, millis());
    return;
  }
  if (reply == "STATUS:RELAY_NODE:READY") {
    showEngineMarkRelayNodeSeen(gState, millis());
    publishNodeState();
    return;
  }

  if (reply.startsWith("RELAY:") && (reply.endsWith(":ON") || reply.endsWith(":OFF"))) {
    int first = reply.indexOf(':');
    int second = reply.indexOf(':', first + 1);
    if (first >= 0 && second > first) {
      uint8_t ch = (uint8_t)reply.substring(first + 1, second).toInt();
      if (ch >= 1 && ch <= SHOW_ENGINE_RELAY_COUNT) {
        /* Status dump — update knowledge without requiring pending */
        RelayKnowledge k = reply.endsWith(":ON") ? RelayKnowledge::On : RelayKnowledge::Off;
        gState.relays[ch - 1].confirmed = k;
        gState.relays[ch - 1].pending = false;
        showEngineBump(gState);
        publishRelayState(ch);
      }
    }
    return;
  }

  if (reply.startsWith(SHOWDUINO_LEGACY_ERR_PREFIX)) {
    /* ERR:RELAY_NODE:EMERGENCY_LOCKED / INVALID_CHANNEL / ... */
    showEngineSetFault(gState, reply.c_str());
    /* If a channel is pending, fail it */
    for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
      if (gState.relays[i].pending) {
        failPending((uint8_t)(i + 1), "NODE_ERROR");
      }
    }
    sendToLink(reply);
    return;
  }

  sendToLink(String(SHOWDUINO_LEGACY_NODE_PREFIX) + reply);
}

void handleRouteErrors(const String &command) {
  if (command == "ERR:RELAY_NODE_MAC_NOT_SET" ||
      command == "ERR:RELAY_NODE_SEND_FAILED") {
    gState.relayNode = (command.indexOf("MAC") >= 0) ? NodeAvailability::Offline
                                                     : NodeAvailability::Fault;
    showEngineBump(gState);
    publishNodeState();
    for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
      if (gState.relays[i].pending) {
        failPending((uint8_t)(i + 1), "ROUTE_ERROR");
      }
    }
    sendToLink(command);
  }
}

void enterEmergency() {
  gState.emergency = EmergencyState::Active;
  gState.show = ShowRuntimeState::Emergency;
  showEngineBump(gState);
  publishEmergencyState();
  publishShowState();
  routeToRelayNode(SHOWDUINO_LEGACY_EMERGENCY_STOP);
  /* Pixel/audio: honest unsupported — no false success */
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:EMERGENCY:STOP");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:EMERGENCY:STOP");
}

void clearEmergency() {
  gState.emergency = EmergencyState::Clear;
  if (gState.show == ShowRuntimeState::Emergency) {
    gState.show = ShowRuntimeState::Idle;
  }
  showEngineBump(gState);
  publishEmergencyState();
  publishShowState();
  routeToRelayNode(SHOWDUINO_LEGACY_EMERGENCY_CLEAR);
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:EMERGENCY:CLEAR");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:EMERGENCY:CLEAR");
}

void handleShowStop() {
  if (gState.emergency == EmergencyState::Active) {
    sendToLink("REJECTED:SHOW:EMERGENCY_ACTIVE");
    return;
  }
  gState.show = ShowRuntimeState::Idle;
  showEngineBump(gState);
  publishShowState();
  /* Absolute safe-state request to relay node */
  for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
    gState.relays[i].pending = true;
    gState.relays[i].pendingOn = false;
    gState.relays[i].pendingSinceMs = millis();
  }
  routeToRelayNode(SHOWDUINO_LEGACY_RELAY_ALL_OFF);
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:ALL:BLACKOUT");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:STOP");
  /* Legacy companion */
  sendToLink(SHOWDUINO_LEGACY_ACK_SHOW_STOP);
}

void handleShowStart() {
  if (gState.emergency == EmergencyState::Active) {
    sendToLink("REJECTED:SHOW:EMERGENCY_ACTIVE");
    return;
  }
  gState.show = ShowRuntimeState::Playing;
  showEngineBump(gState);
  publishShowState();
  sendToLink(SHOWDUINO_LEGACY_ACK_SHOW_START);
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command.startsWith(SHOWDUINO_LEGACY_NODE_PREFIX)) {
    handleNodeReply(command.substring(5));
    return;
  }
  if (command == "ERR:RELAY_NODE_MAC_NOT_SET" ||
      command == "ERR:RELAY_NODE_SEND_FAILED") {
    handleRouteErrors(command);
    return;
  }

  if (looksLikeNoise(command)) {
    noiseBytes += command.length();
    return;
  }

  goodCommands++;

  if (command == SHOWDUINO_LEGACY_HEARTBEAT) {
    sendToLink(SHOWDUINO_LEGACY_ACK_HEARTBEAT);
    return;
  }
  if (command == SHOWDUINO_LEGACY_HELLO) {
    sendToLink(SHOWDUINO_LEGACY_READY);
    sendToLink(SHOWDUINO_LEGACY_SHOWDUINO_STAGE);
    return;
  }

#if LINK_DEBUG
  Serial.print("RX <- link: ");
  Serial.println(command);
#endif

  if (command == SHOWDUINO_LEGACY_STATUS_REQUEST) {
    publishSnapshot();
    /* Refresh from node asynchronously — incremental STATE:RELAY follows */
    if (gState.relayNode != NodeAvailability::Offline) {
      routeToRelayNode(SHOWDUINO_LEGACY_STATUS_REQUEST);
    }
    return;
  }

  if (command == SHOWDUINO_LEGACY_EMERGENCY_STOP) {
    enterEmergency();
    return;
  }

  if (command == SHOWDUINO_LEGACY_EMERGENCY_CLEAR) {
    clearEmergency();
    return;
  }

  if (command == SHOWDUINO_LEGACY_STOP_ALL || command == SHOWDUINO_LEGACY_SHOW_STOP) {
    handleShowStop();
    return;
  }

  if (command == SHOWDUINO_LEGACY_SHOW_START) {
    handleShowStart();
    return;
  }

  if (command.startsWith("SHOW:PAUSE") || command.startsWith("SHOW:RESUME") ||
      command.startsWith("SHOW:LOAD") || command == "SHOW:DEPLOY") {
    sendToLink(String(SHOWDUINO_WIRE_NOT_IMPLEMENTED_PREFIX) + command);
    return;
  }

  if (command.startsWith("RELAY:")) {
    handleRelaySetRequest(command);
    return;
  }

  if (command.startsWith("PIXEL:")) {
    sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + command);
    return;
  }

  if (command.startsWith("AUDIO:")) {
    sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + command);
    return;
  }

  if (command == "SELFTEST:START") {
    if (gState.emergency == EmergencyState::Active) {
      sendToLink("REJECTED:SELFTEST:EMERGENCY_ACTIVE");
      return;
    }
    routeToRelayNode("RELAY:1:PULSE:200");
    sendToLink("ACK:SELFTEST:START");
    return;
  }

  sendToLink("ERR:UNKNOWN_COMMAND");
}

void serviceTimeouts() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
    RelayChannelState &ch = gState.relays[i];
    if (!ch.pending) continue;
    if ((now - ch.pendingSinceMs) >= SHOW_ENGINE_RELAY_PENDING_TIMEOUT_MS) {
      failPending((uint8_t)(i + 1), "TIMEOUT");
    }
  }

  if (gState.lastRelayNodeSeenMs != 0 &&
      gState.relayNode == NodeAvailability::Online &&
      (now - gState.lastRelayNodeSeenMs) >= SHOW_ENGINE_NODE_OFFLINE_TIMEOUT_MS) {
    gState.relayNode = NodeAvailability::Offline;
    showEngineBump(gState);
    publishNodeState();
    for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
      if (gState.relays[i].pending) {
        failPending((uint8_t)(i + 1), "TIMEOUT");
      }
      /* Do not invent OFF — leave confirmed as last known */
    }
  }
}

void readLinkSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        handleCommand(inputBuffer);
        inputBuffer = "";
      }
      continue;
    }

    if (c < 32 || c > 126) {
      noiseBytes++;
      if (inputBuffer.length() > 0) inputBuffer = "";
      continue;
    }

    inputBuffer += c;
    if (inputBuffer.length() > CMD_MAX_LEN) {
      inputBuffer = "";
      noiseBytes++;
    }
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Showduino Show Engine (ESP32-P4)");
  Serial.println("========================================");
  Serial.println("Mode: authoritative hub — relays via C3");

  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  delay(50);
  flushLinkRx();

  Serial.printf("Link UART: RX=%d TX=%d baud=%d\n", LINK_RX_PIN, LINK_TX_PIN, LINK_BAUD);
  Serial.println("Setup complete. Relay states UNKNOWN until confirmed.");

  sendToLink("BOOT:STAGE_ENGINE_READY");
  sendToLink(SHOWDUINO_LEGACY_READY);
  publishShowState();
  sendToLink(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + emergencyWire(gState.emergency));
  publishNodeState();
}

void loop() {
  readLinkSerial();
  serviceTimeouts();
  delay(2);
}
