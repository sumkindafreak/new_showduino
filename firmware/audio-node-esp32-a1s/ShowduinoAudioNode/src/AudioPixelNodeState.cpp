#include "AudioPixelNodeState.h"
#include "AudioPixelEngine.h"

#include <string.h>

static ShowduinoPixelNodeState sState = SHOWDUINO_PIXEL_ST_BOOTING;
static char sFault[24] = "-";
static char sLast[48] = "-";
static char sLastCmd[64] = "-";
static uint32_t sLastComms = 0;
static ShowduinoOwnerMachine sOwner;

void audioPixelNodeStateBegin(ShowduinoPixelNodeState initial) {
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

void audioPixelNodeStateSet(ShowduinoPixelNodeState st) { sState = st; }
ShowduinoPixelNodeState audioPixelNodeState() { return sState; }

void audioPixelNodeStateSetFault(const char *reason) {
  strncpy(sFault, reason && reason[0] ? reason : "FAULT", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sState = SHOWDUINO_PIXEL_ST_FAULT;
  audioPixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT);
}

const char *audioPixelNodeStateFault() { return sFault; }

void audioPixelNodeStateClearFault() {
  strncpy(sFault, "-", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  if (sState == SHOWDUINO_PIXEL_ST_FAULT) {
    sState = SHOWDUINO_PIXEL_ST_SEARCHING;
    audioPixelOwnerApplyEvent(SHOWDUINO_OWNER_EV_FAULT_CLEAR);
  }
}

void audioPixelNodeStateSetLastResult(const char *result) {
  strncpy(sLast, result && result[0] ? result : "-", sizeof(sLast) - 1);
  sLast[sizeof(sLast) - 1] = 0;
}

const char *audioPixelNodeStateLastResult() { return sLast; }

void audioPixelNodeStateSetLastCommand(const char *cmd) {
  strncpy(sLastCmd, cmd && cmd[0] ? cmd : "-", sizeof(sLastCmd) - 1);
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
}

const char *audioPixelNodeStateLastCommand() { return sLastCmd; }

void audioPixelNodeStateNoteComms() { sLastComms = millis(); }
uint32_t audioPixelNodeStateLastCommsMs() { return sLastComms; }

ShowduinoPixelNodeState audioPixelNodeStateDisplay() {
  if (audioPixelEngineEmergency() || audioPixelOwnerMode() == SHOWDUINO_OWNER_EMERGENCY) {
    return SHOWDUINO_PIXEL_ST_EMERGENCY;
  }
  if (audioPixelEngineLocateActive()) return SHOWDUINO_PIXEL_ST_LOCATE;
  if (sState == SHOWDUINO_PIXEL_ST_FAULT) return SHOWDUINO_PIXEL_ST_FAULT;
  if (!audioPixelEngineReady()) return SHOWDUINO_PIXEL_ST_UNINIT;
  return showduino_pixel_state_from_owner(audioPixelOwnerMode());
}

const char *audioPixelNodeStateName() {
  return showduino_pixel_state_name(audioPixelNodeStateDisplay());
}

void audioPixelOwnerTick() {
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

void audioPixelOwnerApplyEvent(ShowduinoOwnerEvent ev) {
  showduino_owner_apply(&sOwner, ev, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

ShowduinoNodeOwnerMode audioPixelOwnerMode() { return sOwner.mode; }
const char *audioPixelOwnerModeName() { return showduino_owner_mode_name(sOwner.mode); }
bool audioPixelOwnerGranted() { return sOwner.granted != 0; }
bool audioPixelOwnerLostAuthority() { return sOwner.lostAuthority != 0; }
bool audioPixelOwnerEnteredStandalone() { return sOwner.enteredStandalone != 0; }
bool audioPixelOwnerEnteredShow() { return sOwner.enteredShow != 0; }
bool audioPixelNodeStateShowControlled() {
  return audioPixelOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}
