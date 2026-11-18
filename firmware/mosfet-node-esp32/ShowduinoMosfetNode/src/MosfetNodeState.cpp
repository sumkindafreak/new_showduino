#include "MosfetNodeState.h"
#include "MosfetOutputEngine.h"

using OwnerLock = MosfetOutputStateLock;

static ShowduinoOwnerMachine sOwner;
static ShowduinoMosfetNodeState sState = SHOWDUINO_MOSFET_ST_BOOTING;
static bool sEnteredShowEdge = false;

void mosfetNodeStateBegin() {
  OwnerLock lock;
  if (!lock) return;
  showduino_owner_begin(&sOwner, millis());
  sState = SHOWDUINO_MOSFET_ST_SEARCHING;
  sEnteredShowEdge = false;
}

void mosfetNodeStateLoop() {
  OwnerLock lock;
  if (!lock) return;
  const uint32_t now = millis();
  const ShowduinoNodeOwnerMode before = sOwner.mode;
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, now,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  if (sOwner.lostAuthority) {
    mosfetOutputEnginePauseForControlLoss();
    sOwner.lostAuthority = 0;
    sEnteredShowEdge = false;
  }
  if (before != SHOWDUINO_OWNER_SHOW_CONTROLLED &&
      sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED) {
    mosfetOutputEngineResumeOnGrant();
    sEnteredShowEdge = true;
  }
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

ShowduinoMosfetNodeState mosfetNodeStateGet() { OwnerLock lock; return lock ? sState : SHOWDUINO_MOSFET_ST_FAULT; }
ShowduinoNodeOwnerMode mosfetNodeOwnerMode() { OwnerLock lock; return lock ? sOwner.mode : SHOWDUINO_OWNER_FAULT; }

void mosfetNodeStateOnGrant(uint32_t nowMs) {
  OwnerLock lock;
  if (!lock) return;
  const ShowduinoNodeOwnerMode before = sOwner.mode;
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_GRANT, nowMs,
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  if (before != SHOWDUINO_OWNER_SHOW_CONTROLLED &&
      sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED) {
    mosfetOutputEngineResumeOnGrant();
    sEnteredShowEdge = true;
  } else {
    showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_KEEP, nowMs,
                          SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  }
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

void mosfetNodeStateOnEmergencyStop() {
  OwnerLock lock;
  if (!lock) { mosfetOutputEngineSafeGpioFirst(); return; }
  mosfetOutputEngineAllOff("EMERGENCY");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_EMERGENCY_STOP, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = SHOWDUINO_MOSFET_ST_EMERGENCY;
}

void mosfetNodeStateOnEmergencyClear() {
  OwnerLock lock;
  if (!lock) return;
  mosfetOutputEngineAllOff("EMERGENCY_CLEAR");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_EMERGENCY_CLEAR, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = showduino_mosfet_state_from_owner(sOwner.mode);
}

void mosfetNodeStateSetFault(const char *reason) {
  OwnerLock lock;
  if (!lock) { mosfetOutputEngineSafeGpioFirst(); return; }
  mosfetOutputEngineAllOff(reason ? reason : "FAULT");
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_FAULT, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sState = SHOWDUINO_MOSFET_ST_FAULT;
}

bool mosfetNodeStateOwned() {
  OwnerLock lock;
  if (!lock) return false;
  return sOwner.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}
bool mosfetNodeStateEmergency() {
  OwnerLock lock;
  if (!lock) return false;
  return sState == SHOWDUINO_MOSFET_ST_EMERGENCY;
}
bool mosfetNodeStateEnteredShow() { OwnerLock lock; return lock && sEnteredShowEdge; }
bool mosfetNodeStateConsumeEnteredShow() {
  OwnerLock lock;
  if (!lock) return false;
  if (!sEnteredShowEdge) return false;
  sEnteredShowEdge = false;
  return true;
}
bool mosfetNodeStateFault() {
  OwnerLock lock;
  if (!lock) return true;
  return sState == SHOWDUINO_MOSFET_ST_FAULT;
}
