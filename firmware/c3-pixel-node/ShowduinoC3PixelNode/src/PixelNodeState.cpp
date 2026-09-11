#include "PixelNodeState.h"
#include "PixelEngine.h"

#include <string.h>

static ShowduinoPixelNodeState sState = SHOWDUINO_PIXEL_ST_BOOTING;
static char sFault[24] = "-";
static char sLast[48] = "-";
static char sLastCmd[64] = "-";
static uint32_t sLastComms = 0;
static ShowduinoOwnerMachine sOwner;

void pixelNodeStateBegin(ShowduinoPixelNodeState initial) {
  sState = initial;
  strncpy(sFault, "-", sizeof(sFault) - 1);
  strncpy(sLast, "-", sizeof(sLast) - 1);
  strncpy(sLastCmd, "-", sizeof(sLastCmd) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sLast[sizeof(sLast) - 1] = 0;
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
  sLastComms = 0;
  showduino_owner_begin(&sOwner, millis());
}

void pixelNodeStateSet(ShowduinoPixelNodeState st) { sState = st; }
ShowduinoPixelNodeState pixelNodeState() { return sState; }

void pixelNodeStateSetFault(const char *reason) {
  strncpy(sFault, reason && reason[0] ? reason : "FAULT", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sState = SHOWDUINO_PIXEL_ST_FAULT;
  pixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT);
}

const char *pixelNodeStateFault() { return sFault; }

void pixelNodeStateClearFault() {
  strncpy(sFault, "-", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  if (sState == SHOWDUINO_PIXEL_ST_FAULT) {
    sState = SHOWDUINO_PIXEL_ST_SEARCHING;
    pixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT_CLEAR);
  }
}

void pixelNodeStateSetLastResult(const char *result) {
  strncpy(sLast, result && result[0] ? result : "-", sizeof(sLast) - 1);
  sLast[sizeof(sLast) - 1] = 0;
}

const char *pixelNodeStateLastResult() { return sLast; }

void pixelNodeStateSetLastCommand(const char *cmd) {
  strncpy(sLastCmd, cmd && cmd[0] ? cmd : "-", sizeof(sLastCmd) - 1);
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
}

const char *pixelNodeStateLastCommand() { return sLastCmd; }

void pixelNodeStateNoteComms() { sLastComms = millis(); }
uint32_t pixelNodeStateLastCommsMs() { return sLastComms; }

ShowduinoPixelNodeState pixelNodeStateDisplay() {
  if (pixelEngineEmergency() || pixelOwnerMode() == SHOWDUINO_OWNER_EMERGENCY) {
    return SHOWDUINO_PIXEL_ST_EMERGENCY;
  }
  if (pixelEngineLocateActive()) return SHOWDUINO_PIXEL_ST_LOCATE;
  if (sState == SHOWDUINO_PIXEL_ST_FAULT) return SHOWDUINO_PIXEL_ST_FAULT;
  if (!pixelEngineReady()) return SHOWDUINO_PIXEL_ST_UNINIT;
  return showduino_pixel_state_from_owner(pixelOwnerMode());
}

const char *pixelNodeStateName() {
  return showduino_pixel_state_name(pixelNodeStateDisplay());
}

void pixelOwnerTick() {
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

void pixelOwnerApplyEvent(ShowduinoOwnerEvent ev) {
  showduino_owner_apply(&sOwner, ev, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

ShowduinoNodeOwnerMode pixelOwnerMode() { return sOwner.mode; }
const char *pixelOwnerModeName() { return showduino_owner_mode_name(sOwner.mode); }
bool pixelOwnerGranted() { return sOwner.granted != 0; }
bool pixelOwnerLostAuthority() { return sOwner.lostAuthority != 0; }
bool pixelOwnerEnteredStandalone() { return sOwner.enteredStandalone != 0; }
bool pixelOwnerEnteredShow() { return sOwner.enteredShow != 0; }
bool pixelNodeStateShowControlled() {
  return pixelOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}
