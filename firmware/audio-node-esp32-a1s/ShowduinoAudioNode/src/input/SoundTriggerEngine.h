#ifndef SHOWDUINO_SOUND_TRIGGER_ENGINE_H
#define SHOWDUINO_SOUND_TRIGGER_ENGINE_H

#include "../../../protocol/showduino_sound_input.h"

void soundTriggerBegin(const ShowduinoSoundInputConfig *cfg);
void soundTriggerApplyConfig(const ShowduinoSoundInputConfig *cfg);
ShowduinoSoundEngine *soundTriggerEngine();
void soundTriggerFeed(uint8_t rms, uint8_t peak, uint32_t nowMs, uint32_t dtMs,
                      int playing, int emergency, int commsOk);
int soundTriggerTakeEvent(ShowduinoSoundEvent *out);
void soundTriggerStartCalibrate(uint32_t nowMs);
void soundTriggerNotePlaybackStop(uint32_t nowMs);
void soundTriggerSetEnabled(bool on);
void soundTriggerSetInputReady(bool ready, bool fault);
void soundTriggerSetLocalTest(bool on);
bool soundTriggerLocalTest();
void soundTriggerEmitTest(uint32_t nowMs);

#endif
