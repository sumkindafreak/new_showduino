#ifndef SHOWDUINO_MOSFET_OUTPUT_ENGINE_H
#define SHOWDUINO_MOSFET_OUTPUT_ENGINE_H

#include <Arduino.h>
#include "../../../protocol/showduino_mosfet_engine.h"

void mosfetOutputEngineBegin();
void mosfetOutputEngineLoop();
void mosfetOutputEngineAllOff(const char *reason);
bool mosfetOutputEngineSetLevel(uint8_t ch1to4, uint8_t level);
bool mosfetOutputEngineOn(uint8_t ch1to4);
bool mosfetOutputEngineOff(uint8_t ch1to4);
bool mosfetOutputEnginePulse(uint8_t ch1to4, uint8_t level, uint32_t ms);
bool mosfetOutputEngineFade(uint8_t ch1to4, uint8_t level, uint32_t ms);
void mosfetOutputEngineLevels(uint8_t out[4]);
void mosfetOutputEngineSafeGpioFirst();

#endif
