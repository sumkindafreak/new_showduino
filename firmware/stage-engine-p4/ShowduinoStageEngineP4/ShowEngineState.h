#ifndef SHOW_ENGINE_STATE_H
#define SHOW_ENGINE_STATE_H

#include <Arduino.h>
#include <stdint.h>
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_legacy_strings.h"

#define SHOW_ENGINE_RELAY_COUNT 8
#define SHOW_ENGINE_RELAY_PENDING_TIMEOUT_MS 3000UL
#define SHOW_ENGINE_NODE_OFFLINE_TIMEOUT_MS  10000UL

enum class ShowRuntimeState : uint8_t {
  Idle = 0,
  Playing,
  Emergency
};

enum class EmergencyState : uint8_t {
  Clear = 0,
  Active = 1
};

enum class RelayKnowledge : uint8_t {
  Unknown = 0,
  Off,
  On,
  Fault
};

enum class NodeAvailability : uint8_t {
  Unknown = 0,
  Online,
  Offline,
  Fault
};

struct RelayChannelState {
  RelayKnowledge confirmed = RelayKnowledge::Unknown;
  bool pending = false;
  bool pendingOn = false;  /* desired absolute state while pending */
  uint16_t pendingSeq = 0;
  uint32_t pendingSinceMs = 0;
};

struct ShowEngineState {
  ShowRuntimeState show = ShowRuntimeState::Idle;
  EmergencyState emergency = EmergencyState::Clear;
  NodeAvailability relayNode = NodeAvailability::Unknown;
  RelayChannelState relays[SHOW_ENGINE_RELAY_COUNT];
  uint32_t revision = 0;
  uint16_t nextRequestSeq = 1;
  uint32_t lastRelayNodeSeenMs = 0;
  uint32_t routedCommands = 0;
  char lastFault[48] = {};
};

inline const char *showRuntimeWire(ShowRuntimeState s) {
  switch (s) {
    case ShowRuntimeState::Playing: return SHOWDUINO_WIRE_SHOW_PLAYING;
    case ShowRuntimeState::Emergency: return SHOWDUINO_WIRE_SHOW_EMERGENCY;
    case ShowRuntimeState::Idle:
    default: return SHOWDUINO_WIRE_SHOW_IDLE;
  }
}

inline const char *emergencyWire(EmergencyState e) {
  return (e == EmergencyState::Active) ? SHOWDUINO_WIRE_EMERGENCY_ACTIVE
                                       : SHOWDUINO_WIRE_EMERGENCY_CLEAR;
}

inline const char *nodeAvailWire(NodeAvailability n) {
  switch (n) {
    case NodeAvailability::Online: return SHOWDUINO_WIRE_NODE_ONLINE;
    case NodeAvailability::Offline: return SHOWDUINO_WIRE_NODE_OFFLINE;
    case NodeAvailability::Fault: return SHOWDUINO_WIRE_NODE_FAULT;
    case NodeAvailability::Unknown:
    default: return SHOWDUINO_WIRE_NODE_UNKNOWN;
  }
}

inline const char *relayKnowledgeWire(RelayKnowledge k) {
  switch (k) {
    case RelayKnowledge::On: return SHOWDUINO_WIRE_RELAY_ON;
    case RelayKnowledge::Off: return SHOWDUINO_WIRE_RELAY_OFF;
    case RelayKnowledge::Fault: return SHOWDUINO_WIRE_RELAY_FAULT;
    case RelayKnowledge::Unknown:
    default: return SHOWDUINO_WIRE_RELAY_UNKNOWN;
  }
}

inline void showEngineBump(ShowEngineState &st) {
  st.revision++;
}

inline void showEngineMarkRelayNodeSeen(ShowEngineState &st, uint32_t nowMs) {
  st.lastRelayNodeSeenMs = nowMs;
  if (st.relayNode != NodeAvailability::Online) {
    st.relayNode = NodeAvailability::Online;
    showEngineBump(st);
  }
}

inline void showEngineSetFault(ShowEngineState &st, const char *msg) {
  if (!msg) msg = "";
  strncpy(st.lastFault, msg, sizeof(st.lastFault) - 1);
  st.lastFault[sizeof(st.lastFault) - 1] = '\0';
}

#endif /* SHOW_ENGINE_STATE_H */
