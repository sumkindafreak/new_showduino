#include "LampNodeState.h"
#include <Arduino.h>
#include <string.h>

static ShowduinoLampNodeState sSt = SHOWDUINO_LAMP_ST_BOOTING;
static char sFault[24] = "-";
static char sLast[32] = "-";
static char sLastCmd[48] = "-";
static bool sShow = false;
static uint32_t sLastComms = 0;

void lampNodeStateBegin(ShowduinoLampNodeState st) {
  sSt = st;
  strncpy(sFault, "-", sizeof(sFault) - 1);
  strncpy(sLast, "-", sizeof(sLast) - 1);
  strncpy(sLastCmd, "-", sizeof(sLastCmd) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sLast[sizeof(sLast) - 1] = 0;
  sLastCmd[sizeof(sLastCmd) - 1] = 0;
  sShow = false;
  sLastComms = 0;
}

void lampNodeStateSet(ShowduinoLampNodeState st) {
  if (sSt == SHOWDUINO_LAMP_ST_EMERGENCY &&
      st != SHOWDUINO_LAMP_ST_EMERGENCY &&
      st != SHOWDUINO_LAMP_ST_STANDALONE &&
      st != SHOWDUINO_LAMP_ST_SHOW_CONTROLLED &&
      st != SHOWDUINO_LAMP_ST_SEARCHING &&
      st != SHOWDUINO_LAMP_ST_FAULT) {
    return;
  }
  sSt = st;
}

ShowduinoLampNodeState lampNodeState() { return sSt; }

void lampNodeStateSetFault(const char *reason) {
  strncpy(sFault, reason && reason[0] ? reason : "FAULT", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  sSt = SHOWDUINO_LAMP_ST_FAULT;
}

const char *lampNodeStateFault() { return sFault; }

void lampNodeStateClearFault() {
  strncpy(sFault, "-", sizeof(sFault) - 1);
  sFault[sizeof(sFault) - 1] = 0;
  if (sSt == SHOWDUINO_LAMP_ST_FAULT) {
    sSt = sShow ? SHOWDUINO_LAMP_ST_SHOW_CONTROLLED : SHOWDUINO_LAMP_ST_SEARCHING;
  }
}

void lampNodeStateSetShowControlled(bool v) { sShow = v; }
bool lampNodeStateShowControlled() { return sShow; }

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
  if (sLastComms == 0) return false;
  return (millis() - sLastComms) < timeoutMs;
}

const char *lampNodeStateName() { return showduino_lamp_state_name(sSt); }
