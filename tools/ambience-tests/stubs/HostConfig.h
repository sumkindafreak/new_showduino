#pragma once
// Supply only hardware/storage dependencies; compile the actual player below.
#define SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_STORAGE_H
#include "Arduino.h"
#include "FS.h"
#define P4_AMBIENCE_I2S_WS 20
#define P4_AMBIENCE_I2S_BCLK 21
#define P4_AMBIENCE_I2S_DOUT 22
#define PATH_SYSTEM_EMERGENCY_WAV "/showduino/audio/system/emergency.wav"
#define PATH_AUDIO_SYSTEM_LIBRARY "/showduino/audio/show_machine"
extern fs::FS hostSd;
extern bool hostSdReady;
inline bool stageStorageIsReady(){return hostSdReady;}
inline fs::FS& stageStorageFs(){return hostSd;}
