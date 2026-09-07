#include "AudioNodeState.h"

static ShowduinoAudioNodeState sState = SHOWDUINO_AUDIO_ST_UNKNOWN;
static ShowduinoAudioFail sFault = SHOWDUINO_AUDIO_FAIL_NONE;
static bool sShow = false;
static uint32_t sLastComms = 0;

void audioNodeStateBegin(ShowduinoAudioNodeState initial) {
  sState = initial;
  sFault = SHOWDUINO_AUDIO_FAIL_NONE;
  sShow = false;
  sLastComms = 0;
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

void audioNodeStateSetShowControlled(bool on) { sShow = on; }
bool audioNodeStateShowControlled() { return sShow; }

void audioNodeStateNoteComms() { sLastComms = millis(); }
uint32_t audioNodeStateLastCommsMs() { return sLastComms; }

bool audioNodeStateAuthorityFresh(uint32_t timeoutMs) {
  if (sLastComms == 0) return false;
  return (millis() - sLastComms) < timeoutMs;
}

const char *audioNodeStateName() {
  return showduino_audio_state_name(sState);
}
