#ifndef SHOWDUINO_STAGE_LOG_H
#define SHOWDUINO_STAGE_LOG_H

#include <Arduino.h>

enum class StageLogChannel : uint8_t {
  System = 0,
  Comms,
  Network,
  Emergency,
  Production,
  Plugin
};

void stageLogBegin();
void stageLogLoop();
void stageLogWrite(StageLogChannel channel, const char *level, const char *message);
void stageLogEmergency(const char *event, const char *detail);
uint32_t stageLogUsedBytes();

#endif
