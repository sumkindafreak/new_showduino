#ifndef SHOWDUINO_PIXEL_ENGINE_H
#define SHOWDUINO_PIXEL_ENGINE_H

#include <Arduino.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include <algorithm>
#include "../../../protocol/showduino_pixel_fx.h"
#include "../../../protocol/showduino_pixel_node.h"

using std::min;
using std::max;

bool audioPixelEngineSafeGpio();
bool audioPixelEngineBegin();
void audioPixelEngineApplyPersisted();
void audioPixelEngineService();
void audioPixelEngineBlackout();
void audioPixelEngineOnEmergency(bool active);
bool audioPixelEngineReady();
bool audioPixelEngineEmergency();
bool audioPixelEngineLocateActive();
void audioPixelEngineStartLocate();
uint16_t audioPixelEngineCount();
uint16_t audioPixelEngineConfiguredCount();
uint16_t audioPixelEngineMax();
uint8_t audioPixelEngineGlobalBrightness();
uint8_t audioPixelEngineActiveSegments();
int audioPixelEnginePin();
void audioPixelEngineSetConfiguredCount(uint16_t count);
void audioPixelEngineRelease();
void audioPixelEnginePrintStatus();
bool audioPixelEngineHandleCommand(const char *command, char *reply, size_t replyLen);

#endif
