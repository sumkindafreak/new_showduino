#ifndef SHOWDUINO_LAMP_ENGINE_H
#define SHOWDUINO_LAMP_ENGINE_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../../../protocol/showduino_lamp_node.h"

bool lampEngineBegin();
void lampEngineService();
void lampEngineOff();
void lampEngineSetBrightness(uint8_t bri0to100);
uint8_t lampEngineBrightness();
void lampEngineSetSolid(uint8_t r, uint8_t g, uint8_t b);
void lampEngineSetFx(ShowduinoLampFx fx, uint8_t speed, uint8_t intensity);
ShowduinoLampFx lampEngineFx();
const char *lampEngineFxDisplay();
const char *lampEngineFxToken();
bool lampEngineActive();
void lampEngineOnEmergency(bool active);
bool lampEngineEmergency();
void lampEngineTest();
void lampEngineFill(uint8_t r, uint8_t g, uint8_t b);
uint16_t lampEngineCount();
int lampEnginePin();

#endif
