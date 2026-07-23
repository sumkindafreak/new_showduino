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

  microSD (optional): SPI pins in BoardConfig.h
      Defaults: SCK=43 MISO=39 MOSI=44 CS=42 PWR=45
      Layout: /showduino/www , /showduino/shows , ...
*/

#include <Arduino.h>
#include "../../../protocol/showduino_protocol_version.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_show_runtime.h"
#include "../../../protocol/showduino_web_tunnel.h"
#include "BoardConfig.h"
#include "ShowEngineState.h"
#include "ShowRuntimeOwner.h"
#include "src/StageStorage.h"
#include "src/EmergencyPixels.h"
#include "src/WebApiHandler.h"

#define DEBUG_BAUD 115200
#define LINK_BAUD  115200
#define LINK_RX_PIN 5
#define LINK_TX_PIN 6
#define LINK_DEBUG 0
#define CMD_MAX_LEN SHOWDUINO_DESK_COMMAND_MAX

ShowEngineState gState;
ShowRuntimeOwner gRuntime;
String inputBuffer;
uint32_t noiseBytes = 0;
uint32_t goodCommands = 0;

void handleCommand(String command);
void timelineCueDispatch(const char *command);

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

void sendToLinkCStr(const char *message) {
  if (!message || !message[0]) return;
  sendToLink(String(message));
}

void timelineCueDispatch(const char *command) {
  if (!command || !command[0]) return;
  handleCommand(String(command));
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
  gRuntime.syncLegacyShow(&gState);
  sendToLink(String(SHOWDUINO_WIRE_STATE_SHOW_PREFIX) + showRuntimeWire(gState.show));
  gRuntime.broadcastAll();
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
  /* Existing safety path unchanged — runtime mirrors pause + EMERGENCY_STOP. */
  gState.emergency = EmergencyState::Active;
  showEngineBump(gState);
  gRuntime.onEmergencyStop(millis(), &gState);
  publishEmergencyState();
  publishShowState();
  routeToRelayNode(SHOWDUINO_LEGACY_EMERGENCY_STOP);

  /* Local Stage indicator: single Neopixel line → solid white. */
  emergencyPixelsSetWhite();

  /* Remote pixel/audio nodes still honest-unsupported until those engines exist. */
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:EMERGENCY:STOP");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:EMERGENCY:STOP");
}

void clearEmergency() {
  gState.emergency = EmergencyState::Clear;
  showEngineBump(gState);
  gRuntime.onEmergencyCleared(millis(), &gState);
  publishEmergencyState();
  publishShowState();
  routeToRelayNode(SHOWDUINO_LEGACY_EMERGENCY_CLEAR);

  emergencyPixelsBlackout();

  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:EMERGENCY:CLEAR");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:EMERGENCY:CLEAR");
}

void handleShowStop() {
  const bool abortFromEstop =
      (gState.emergency == EmergencyState::Active) ||
      (gRuntime.rt.state == SHOW_STATE_EMERGENCY_STOP);

  if (!gRuntime.handleStop(millis(), &gState)) return;
  showEngineBump(gState);

  /* Absolute safe-state request to relay node */
  for (uint8_t i = 0; i < SHOW_ENGINE_RELAY_COUNT; i++) {
    gState.relays[i].pending = true;
    gState.relays[i].pendingOn = false;
    gState.relays[i].pendingSinceMs = millis();
  }
  routeToRelayNode(SHOWDUINO_LEGACY_RELAY_ALL_OFF);
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:ALL:BLACKOUT");
  sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:STOP");

  /* Abort Show from E-stop: publish CLEAR so Director can leave safe-mode UI. */
  if (abortFromEstop) {
    emergencyPixelsBlackout();
    publishEmergencyState();
    publishShowState();
    routeToRelayNode(SHOWDUINO_LEGACY_EMERGENCY_CLEAR);
    sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "PIXEL:EMERGENCY:CLEAR");
    sendToLink(String(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) + "AUDIO:EMERGENCY:CLEAR");
  }
}

void handleShowStart() {
  gRuntime.handleRun(millis(), &gState);
  showEngineBump(gState);
}

void handleShowLoad(const String &command) {
  const char *name = "";
  if (command.startsWith("SHOW:LOAD:")) {
    name = command.c_str() + strlen("SHOW:LOAD:");
  } else if (command == "SHOW:LOAD" || command == "SHOW:DEPLOY") {
    name = gRuntime.rt.showName[0] ? gRuntime.rt.showName : "show";
  }
  gRuntime.handleLoadName(name, millis(), &gState);
  showEngineBump(gState);
}

void handleTlCommand(const String &command) {
  uint32_t now = millis();
  if (command == "SHOW:TL:BEGIN") {
    gRuntime.handleTlBegin();
    return;
  }
  if (command == "SHOW:TL:END") {
    gRuntime.handleTlEnd(now, &gState);
    showEngineBump(gState);
    return;
  }
  if (command.startsWith("SHOW:TL:C:")) {
    /* SHOW:TL:C:<ms>:<cmd> */
    const char *p = command.c_str() + strlen("SHOW:TL:C:");
    char *end = nullptr;
    unsigned long ms = strtoul(p, &end, 10);
    if (!end || *end != ':' || !end[1]) {
      gRuntime.setError("bad_tl_cue", now);
      return;
    }
    if (!gRuntime.handleTlCue((uint32_t)ms, end + 1)) {
      sendToLink("REJECTED:SHOW:TL:C");
    }
    return;
  }
  sendToLink(String(SHOWDUINO_WIRE_NOT_IMPLEMENTED_PREFIX) + command);
}

/* Always answer WEB/ so C3 never HTTP-502s if WebApiHandler was stubbed. */
static void sendWebTunnelFallback(const char *body) {
  const size_t n = strlen(body);
  Serial1.print(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  Serial1.print(200);
  Serial1.print(':');
  Serial1.print((unsigned)n);
  Serial1.print('\n');
  Serial1.print(body);
  Serial1.flush();
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  /* Studio API tunnel (C3 Wi-Fi → UART → here). Must run even if WebUI flag off. */
  if (command.startsWith("WEB/")) {
    Serial.print("[WebAPI] tunnel ");
    Serial.println(command);
#if SHOWDUINO_WEBUI_ENABLED
    if (webApiHandleTunnelRequest(command)) return;
#endif
    sendWebTunnelFallback("{\"role\":\"show-engine\",\"webApi\":false,\"hint\":\"reflash P4 with WebAPI\"}\n");
    return;
  }

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
    gRuntime.handleStateQuery();
    /* Refresh from node asynchronously — incremental STATE:RELAY follows */
    if (gState.relayNode != NodeAvailability::Offline) {
      routeToRelayNode(SHOWDUINO_LEGACY_STATUS_REQUEST);
    }
    return;
  }

  if (command == SHOW_STATE_QUERY || command == "SHOW:STATE" || command == "SHOW:STATUS") {
    gRuntime.handleStateQuery();
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

  /* STOP aliases — Abort Show must always hit handleShowStop. */
  if (command == SHOWDUINO_LEGACY_STOP_ALL || command == SHOWDUINO_LEGACY_SHOW_STOP ||
      command == "SHOW:ABORT" || command == "ABORT:SHOW") {
    handleShowStop();
    return;
  }

  if (command == SHOWDUINO_LEGACY_SHOW_START || command == "SHOW:RUN" ||
      command.startsWith("SHOW:RUN:")) {
    handleShowStart();
    return;
  }

  if (command == "SHOW:PAUSE") {
    gRuntime.handlePause(millis(), &gState);
    return;
  }
  if (command == "SHOW:RESUME") {
    gRuntime.handleResume(millis(), &gState);
    return;
  }

  if (command.startsWith("SHOW:TL:")) {
    handleTlCommand(command);
    return;
  }

  if (command.startsWith("SHOW:LOAD") || command == "SHOW:DEPLOY") {
    handleShowLoad(command);
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

  sendToLink(String("ERR:UNKNOWN_COMMAND:") + command);
  Serial.print("[Stage] unknown cmd: ");
  Serial.println(command);
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
    if (inputBuffer.length() > 160) {
      inputBuffer = "";
      noiseBytes++;
    }
  }
}

unsigned long bootMs = 0;

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(500);
  bootMs = millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println(" Showduino Show Engine (ESP32-P4)");
  Serial.println("========================================");
  Serial.println("Mode: authoritative hub — ShowRuntime + relays via C3");

  Serial1.setRxBufferSize(4096);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  delay(50);
  flushLinkRx();

  gRuntime.begin(sendToLinkCStr);
  gRuntime.setDispatch(timelineCueDispatch);
  /* No interrupted-show recovery yet — boot IDLE. */
  gRuntime.bootToIdle();

  Serial.printf("Link UART: RX=%d TX=%d baud=%d\n", LINK_RX_PIN, LINK_TX_PIN, LINK_BAUD);

#if SHOWDUINO_SD_ENABLED
  if (stageStorageBegin()) {
    Serial.println("[Storage] Stage SD online");
  } else {
    Serial.println("[Storage] Continuing without SD (retry in loop)");
  }
#endif

#if SHOWDUINO_EMERGENCY_PIXEL_ENABLED
  emergencyPixelsBegin();
#endif

  Serial.println("Setup complete. Relay states UNKNOWN until confirmed.");

  sendToLink("BOOT:STAGE_ENGINE_READY");
  sendToLink(SHOWDUINO_LEGACY_READY);
  publishShowState();
  sendToLink(String(SHOWDUINO_WIRE_STATE_EMERGENCY_PREFIX) + emergencyWire(gState.emergency));
  publishNodeState();

#if SHOWDUINO_WEBUI_ENABLED
  webApiBegin(bootMs);
  Serial.println("WebAPI: REST handlers ready (C3 Wi-Fi front door)");
#endif
}

void loop() {
  readLinkSerial();
  serviceTimeouts();
  gRuntime.service(millis(), &gState);
#if SHOWDUINO_SD_ENABLED
  stageStorageLoop();
#endif
}
