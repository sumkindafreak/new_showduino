#include "AudioNodeState.h"

static ShowduinoAudioNodeState sState = SHOWDUINO_AUDIO_ST_UNKNOWN;
static ShowduinoAudioFail sFault = SHOWDUINO_AUDIO_FAIL_NONE;
static uint32_t sLastComms = 0;
static ShowduinoOwnerMachine sOwner;

void audioNodeStateBegin(ShowduinoAudioNodeState initial) {
  sState = initial;
  sFault = SHOWDUINO_AUDIO_FAIL_NONE;
  sLastComms = 0;
  showduino_owner_begin(&sOwner, millis());
}

void audioNodeStateSet(ShowduinoAudioNodeState st) {
  sState = st;
}

ShowduinoAudioNodeState audioNodeState() { return sState; }

void audioNodeStateSetFault(ShowduinoAudioFail fault) {
  sFault = fault;
  sState = (fault == SHOWDUINO_AUDIO_FAIL_NO_STORAGE) ? SHOWDUINO_AUDIO_ST_NO_STORAGE
                                                      : SHOWDUINO_AUDIO_ST_FAULT;
}

ShowduinoAudioFail audioNodeStateFault() { return sFault; }

void audioNodeStateClearFault() {
  if (sState == SHOWDUINO_AUDIO_ST_FAULT) sState = SHOWDUINO_AUDIO_ST_IDLE;
  sFault = SHOWDUINO_AUDIO_FAIL_NONE;
}

void audioNodeStateSetShowControlled(bool on) {
  (void)on;
}

bool audioNodeStateShowControlled() {
  return audioOwnerMode() == SHOWDUINO_OWNER_SHOW_CONTROLLED;
}

void audioNodeStateNoteComms() { sLastComms = millis(); }
uint32_t audioNodeStateLastCommsMs() { return sLastComms; }

bool audioNodeStateAuthorityFresh(uint32_t timeoutMs) {
  if (!sOwner.granted || sOwner.lastGrantMs == 0) return false;
  return (millis() - sOwner.lastGrantMs) < timeoutMs;
}

const char *audioNodeStateName() {
  return showduino_audio_state_name(sState);
}

void audioOwnerTick() {
  showduino_owner_apply(&sOwner, SHOWDUINO_OWNER_EV_TICK, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

void audioOwnerApplyEvent(ShowduinoOwnerEvent ev) {
  showduino_owner_apply(&sOwner, ev, millis(),
                        SHOWDUINO_OWNER_DISCOVER_MS, SHOWDUINO_OWNER_KEEPALIVE_MS);
}

ShowduinoNodeOwnerMode audioOwnerMode() { return sOwner.mode; }
const char *audioOwnerModeName() { return showduino_owner_mode_name(sOwner.mode); }
bool audioOwnerGranted() { return sOwner.granted != 0; }
bool audioOwnerLostAuthority() { return sOwner.lostAuthority != 0; }
bool audioOwnerEnteredStandalone() { return sOwner.enteredStandalone != 0; }
bool audioOwnerEnteredShow() { return sOwner.enteredShow != 0; }
