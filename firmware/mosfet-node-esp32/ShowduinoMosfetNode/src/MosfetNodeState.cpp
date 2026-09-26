#include "MosfetNodeState.h"
#include "MosfetOutputEngine.h"

static ShowduinoOwnerMachine sOwner;
static ShowduinoMosfetNodeState sState = SHOWDUINO_MOSFET_ST_BOOTING;
static bool sEnteredShowEdge = false;

void mosfetNodeStateBegin() {
  showduino_owner_begin(&sOwner, millis());
  sState = SHOWDUINO_MOSFET_ST_SEARCHING;
  sEnteredShowEdge = false;
}

void mosfetNodeStateLoop() {
  const uint32_t now = millis();
  const ShowduinoNodeOwnerMode before = sOwner.mode;
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, now,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  if (sOwner.lostAuthority) {
    mosfetOutputEngineAllOff("AUTHORITY_LOST");
    sOwner.lostAuthority = 0;
    sEnteredShowEdge = false;
  }
  if (before != SHOWDUINO_OWNER_SHOW_CONTROLLED &&
      sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED) {
    mosfetOutputEngineAllOff("FIRST_GRANT");
    sEnteredShowEdge = true;
  }
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

ShowduinoMosfetNodeState mosfetNodeStateGet() { return sState; }
ShowduinoNodeOwnerMode mosfetNodeOwnerMode() { return sOwner.mode; }

void mosfetNodeStateOnGrant(uint32_t nowMs) {
  const ShowduinoNodeOwnerMode before = sOwner.mode;
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_GRANT, nowMs,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  if (before != SHOWDUINO_OWNER_SHOW_CONTROLLED &&
      sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED) {
    mosfetOutputEngineAllOff("FIRST_GRANT");
  } else {
    showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_KEEP, nowMs,
                          SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  }
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

void mosfetNodeStateOnEmergencyStop() {
  mosfetOutputEngineAllOff("EMERGENCY");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_EMERGENCY_STOP, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = SHOWDUINO_MOSFET_ST_EMERGENCY;
}

void mosfetNodeStateOnEmergencyClear() {
  mosfetOutputEngineAllOff("EMERGENCY_CLEAR");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

void mosfetNodeStateSetFault(const char *reason) {
  mosfetOutputEngineAllOff(reason ? reason : "FAULT");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_FAULT, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = SHOWDUINO_MOSFET_ST_FAULT;
}

bool mosfetNodeStateOwned() {
  return sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}
bool mosfetNodeStateEmergency() {
  return sState == SHOWDUINO_MOSFET_ST_EMERGENCY;
}
bool mosfetNodeStateEnteredShow() { return sEnteredShowEdge; }
