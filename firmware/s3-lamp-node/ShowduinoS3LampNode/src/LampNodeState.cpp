#include "LampNodeState.h"

#include <Arduino.h>
#include <string.h>

static ShowduinoLampNodeState sSt = SHOWDUINO_LAMP_ST_BOOTING;
static char sFault[24] = "-";
static char sLast[32] = "-";
static char sLastCmd[48] = "-";
static uint32_t sLastComms = 0;
static ShowduinoOwnerMachine sOwner;

void lampNodeStateBegin(ShowduinoLampNodeState st) {
  (void)st;
  strncpy(sFault, "-", sizeof(sFault) - 1);
  strncpy(sLast, "-", sizeof(sLast) - 1);
  strncpy(sLastCmd, "-", sizeof(sLastCmd) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sLast[sizeof(sLast) - 1] = 0;
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
  sLastComms = 0;
  showduino_owner_begin(&sOwner, millis());
  sSt = showduino_lamp_state_from_owner(sOwner.mode);
}

void lampOwnerTick() {
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sSt = showduino_lamp_state_from_owner(sOwner.mode);
}

void lampOwnerApplyEvent(ShowduinoOwnerEvent ev) {
  showduino_owner_apply(&sOwner, ev, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
  sSt = showduino_lamp_state_from_owner(sOwner.mode);
}

ShowduinoNodeOwnerMode lampOwnerMode() { return sOwner.mode; }
const char *lampOwnerModeName() { return showduino_owner_mode_name(sOwner.mode); }
bool lampOwnerGranted() { return sOwner.granted != 0; }
bool lampOwnerLostAuthority() { return sOwner.lostAuthority != 0; }
bool lampOwnerEnteredStandalone() { return sOwner.enteredStandalone != 0; }
bool lampOwnerEnteredShow() { return sOwner.enteredShow != 0; }
uint32_t lampOwnerLastGrantMs() { return sOwner.lastGrantMs; }

void lampNodeStateSet(ShowduinoLampNodeState st) {
  if (st == SHOWDUINO_LAMP_ST_EMERGENCY) {
    lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_EMERGENCY_STOP);
    return;
  }
  if (st == SHOWDUINO_LAMP_ST_FAULT) {
    lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT);
    return;
  }
  if (st == SHOWDUINO_LAMP_ST_SHOW_CONTROLLED) {
    lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_GRANT);
    return;
  }
  /* SEARCHING / STANDALONE are owned by the GRANT/discover machine. */
}

ShowduinoLampNodeState lampNodeState() { return sSt; }

void lampNodeStateSetFault(const char *reason) {
  strncpy(sFault, reason && reason[0] ? reason : "FAULT", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT);
}

const char *lampNodeStateFault() { return sFault; }

void lampNodeStateClearFault() {
  strncpy(sFault, "-", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT_CLEAR);
}

void lampNodeStateSetShowControlled(bool v) {
  if (v) lampOwnerApplyEvent(SHOWDUINO_OWNER_EV_GRANT);
}

bool lampNodeStateShowControlled() {
  return lampOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}

void lampNodeStateSetLastResult(const char *result) {
  strncpy(sLast, result && result[0] ? result : "-", sizeof(sLast) - 1);
  sLast[sizeof(sLast) - 1] = 0;
}

const char *lampNodeStateLastResult() { return sLast; }

void lampNodeStateSetLastCommand(const char *cmd) {
  strncpy(sLastCmd, cmd && cmd[0] ? cmd : "-", sizeof(sLastCmd) - 1);
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
}

const char *lampNodeStateLastCommand() { return sLastCmd; }

void lampNodeStateNoteComms() { sLastComms = millis(); }
uint32_t lampNodeStateLastCommsMs() { return sLastComms; }

bool lampNodeStateAuthorityFresh(uint32_t timeoutMs) {
  return showduino_lamp_grant_fresh(sOwner.granted ? 1 : 0, millis(),
                                    sOwner.lastGrantMs, timeoutMs) != 0;
}

const char *lampNodeStateName() { return showduino_lamp_state_name(lampNodeState()); }

ShowduinoLampProductMode lampNodeProductMode() {
  return showduino_lamp_product_mode(lampNodeState());
}

const char *lampNodeProductModeName() {
  return showduino_lamp_product_mode_name(lampNodeProductMode());
}
