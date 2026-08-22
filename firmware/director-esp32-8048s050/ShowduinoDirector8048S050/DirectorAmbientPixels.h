#ifndef SHOWDUINO_DIRECTOR_AMBIENT_PIXELS_H
#define SHOWDUINO_DIRECTOR_AMBIENT_PIXELS_H

#include <Arduino.h>
#include "BoardConfig.h"
#include "../../../protocol/showduino_show_runtime.h"

/**
 * Director ambient NeoPixel mood lighting.
 * Driven from link + mirrored ShowRuntime + emergency lock.
 * Non-blocking: call directorAmbientBegin() once, directorAmbientLoop() each loop.
 */

enum DirectorAmbientMode : uint8_t {
  DIRECTOR_AMBIENT_OFF = 0,
  DIRECTOR_AMBIENT_BOOT,
  DIRECTOR_AMBIENT_IDLE,
  DIRECTOR_AMBIENT_READY,
  DIRECTOR_AMBIENT_RUNNING,
  DIRECTOR_AMBIENT_WARNING,
  DIRECTOR_AMBIENT_EMERGENCY,
  DIRECTOR_AMBIENT_FAULT,
  DIRECTOR_AMBIENT_SUCCESS
};

void directorAmbientBegin();
void directorAmbientLoop(uint32_t nowMs);

/** Push Director operational context (call when state may have changed). */
void directorAmbientSync(uint8_t linkState,
                         ShowState showState,
                         bool emergencyLocked,
                         bool stageConnected);

void directorAmbientSetBrightness(uint8_t brightness);
uint8_t directorAmbientBrightness();
DirectorAmbientMode directorAmbientMode();
bool directorAmbientReady();

#endif
