#include "SoundTriggerEngine.h"

static ShowduinoSoundEngine sEng;
static bool sLocalTest = false;

void soundTriggerBegin(const ShowduinoSoundInputConfig *cfg) {
  showduino_sound_engine_init(&sEng, cfg);
}

void soundTriggerApplyConfig(const ShowduinoSoundInputConfig *cfg) {
  if (!cfg) return;
  sEng.cfg = *cfg;
  if (!cfg->enabled) sEng.armed = 0;
}

ShowduinoSoundEngine *soundTriggerEngine() { return &sEng; }

void soundTriggerFeed(uint8_t rms, uint8_t peak, uint32_t nowMs, uint32_t dtMs,
                      int playing, int emergency, int commsOk) {
  showduino_sound_engine_feed(&sEng, rms, peak, nowMs, dtMs, playing, emergency, commsOk);
}

int soundTriggerTakeEvent(ShowduinoSoundEvent *out) {
  return showduino_sound_engine_take_event(&sEng, out);
}

void soundTriggerStartCalibrate(uint32_t nowMs) {
  showduino_sound_engine_start_calibrate(&sEng, nowMs);
}

void soundTriggerNotePlaybackStop(uint32_t nowMs) {
  showduino_sound_engine_note_playback_stop(&sEng, nowMs);
}

void soundTriggerSetEnabled(bool on) {
  showduino_sound_engine_set_enabled(&sEng, on ? 1 : 0);
}

void soundTriggerSetInputReady(bool ready, bool fault) {
  sEng.inputReady = ready ? 1 : 0;
  sEng.inputFault = fault ? 1 : 0;
}

void soundTriggerSetLocalTest(bool on) { sLocalTest = on; }
bool soundTriggerLocalTest() { return sLocalTest; }

void soundTriggerEmitTest(uint32_t nowMs) {
  sEng.smoothed = sEng.smoothed ? sEng.smoothed : 50;
  sEng.instPeak = sEng.instPeak ? sEng.instPeak : 60;
  showduino_sound_emit(&sEng, SHOWDUINO_SOUND_EVT_TRANSIENT, nowMs);
}
