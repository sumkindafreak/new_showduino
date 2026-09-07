#ifndef SHOWDUINO_AUDIO_LEVEL_METER_H
#define SHOWDUINO_AUDIO_LEVEL_METER_H

#include <stdint.h>
#include <stddef.h>

void audioLevelMeterReset();
void audioLevelMeterAddFrames(const int16_t *stereo, size_t frames);
uint8_t audioLevelMeterRms();
uint8_t audioLevelMeterPeak();
void audioLevelMeterEndBlock();

#endif
