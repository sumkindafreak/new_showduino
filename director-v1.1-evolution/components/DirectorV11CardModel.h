#ifndef SHOWDUINO_DIRECTOR_V11_CARD_MODEL_H
#define SHOWDUINO_DIRECTOR_V11_CARD_MODEL_H

// Read-only display classification. P4/node state must supply these inputs.
// Never turn this model's result into permission to run an effect or clear E-stop.

#include <cstdint>

namespace ShowduinoV11 {

enum class CardStatus : uint8_t {
  Emergency,
  LinkOffline,
  Fault,
  Stale,
  AwaitingState,
  Ready
};

struct NodeEvidence {
  bool emergencyActive = false;
  bool commsOnline = false;
  bool stateReceived = false;
  bool nodeReportsFault = false;
  bool nodeReportsOnline = false;
  bool p4Authorised = false;
  uint32_t stateAgeMs = 0;
};

// Only the current verified authority/state should be passed in. A sent
// command or transport ACK does not count as nodeReportsOnline/p4Authorised.
inline CardStatus classifyNode(const NodeEvidence &e, uint32_t staleAfterMs = 6000) {
  if (e.emergencyActive) return CardStatus::Emergency;
  if (!e.commsOnline) return CardStatus::LinkOffline;
  if (!e.stateReceived) return CardStatus::AwaitingState;
  if (e.nodeReportsFault) return CardStatus::Fault;
  if (e.stateAgeMs > staleAfterMs) return CardStatus::Stale;
  if (!e.nodeReportsOnline || !e.p4Authorised) return CardStatus::AwaitingState;
  return CardStatus::Ready;
}

inline const char *statusLabel(CardStatus s) {
  switch (s) {
    case CardStatus::Emergency: return "EMERGENCY";
    case CardStatus::LinkOffline: return "LINK OFFLINE";
    case CardStatus::Fault: return "FAULT";
    case CardStatus::Stale: return "STALE STATE";
    case CardStatus::AwaitingState: return "AWAITING P4 STATE";
    case CardStatus::Ready: return "READY";
  }
  return "UNKNOWN";
}

// Colour choices are suggestions for the UI only; never the source of safety.
inline uint32_t statusColorRgb(CardStatus s) {
  switch (s) {
    case CardStatus::Emergency: return 0xF04A48;
    case CardStatus::LinkOffline: return 0xA7A7A7;
    case CardStatus::Fault: return 0xF04A48;
    case CardStatus::Stale: return 0xE6AA43;
    case CardStatus::AwaitingState: return 0xE6AA43;
    case CardStatus::Ready: return 0x35BD81;
  }
  return 0xA7A7A7;
}

}  // namespace ShowduinoV11

#endif  // SHOWDUINO_DIRECTOR_V11_CARD_MODEL_H
