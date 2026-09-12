#ifndef SHOWDUINO_S3_LAMP_ENGINE_H
#define SHOWDUINO_S3_LAMP_ENGINE_H

#include <Arduino.h>
#include "../../../protocol/showduino_carbide_lamp.h"

void lampEngineBegin();
void lampEngineService();
void lampEngineApplyEvent(ShowduinoCarbideEvent ev);
void lampEngineSetBrightness(uint8_t bri0to100);
uint8_t lampEngineBrightness();
void lampEngineSetSolid(uint8_t r, uint8_t g, uint8_t b);
void lampEngineSetCompatFx(ShowduinoLampFx fx, uint8_t speed, uint8_t intensity);
ShowduinoLampFx lampEngineFx();
const char *lampEngineFxToken();
const char *lampEngineFxDisplay();
ShowduinoCarbideState lampEngineCarbide();
const char *lampEngineCarbideName();
bool lampEngineActive();
bool lampEngineFlameLit();
void lampEngineOnEmergency(bool active);
bool lampEngineEmergency();
void lampEngineFill(uint8_t r, uint8_t g, uint8_t b);
uint16_t lampEngineCount();
int lampEnginePin();
bool lampEngineJewelReady();
void lampEnginePixelRgb(uint8_t i, uint8_t *r, uint8_t *g, uint8_t *b);

#endif
