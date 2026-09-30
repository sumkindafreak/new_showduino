#ifndef SHOWDUINO_STAGE_AMBIENCE_H
#define SHOWDUINO_STAGE_AMBIENCE_H
#include <Arduino.h>
struct StageAmbienceStatus {
  bool i2sReady=false, playing=false, looping=false, emergencyBlocked=false;
  uint8_t volume=80;
  uint32_t sampleRate=0;
  char path[96]="";
  char error[64]="not started";
};
bool stageAmbienceBegin();
void stageAmbienceLoop();
void stageAmbienceOnEmergency(bool active);
void stageAmbienceStop();
const StageAmbienceStatus &stageAmbienceStatus();
bool stageAmbienceHandleCommand(const char *command, char *reply, size_t replyLen);
#endif
