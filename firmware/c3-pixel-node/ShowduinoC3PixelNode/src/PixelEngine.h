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

bool pixelEngineSafeGpio();
bool pixelEngineBegin();
void pixelEngineApplyPersisted();
void pixelEngineService();
void pixelEngineBlackout();
void pixelEngineOnEmergency(bool active);
bool pixelEngineReady();
bool pixelEngineEmergency();
bool pixelEngineLocateActive();
void pixelEngineStartLocate();
uint16_t pixelEngineCount();
uint16_t pixelEngineConfiguredCount();
uint16_t pixelEngineMax();
uint8_t pixelEngineGlobalBrightness();
uint8_t pixelEngineActiveSegments();
int pixelEnginePin();
void pixelEngineSetConfiguredCount(uint16_t count);
void pixelEngineRelease();
void pixelEnginePrintStatus();
bool pixelEngineHandleCommand(const char *command, char *reply, size_t replyLen);

#endif
