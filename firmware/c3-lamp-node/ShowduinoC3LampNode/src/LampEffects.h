#ifndef SHOWDUINO_LAMP_EFFECTS_H
#define SHOWDUINO_LAMP_EFFECTS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../../../protocol/showduino_lamp_node.h"

void lampEffectsReset(ShowduinoLampFx fx);
void lampEffectsTick(Adafruit_NeoPixel &strip, ShowduinoLampFx fx,
                     uint8_t bri0to100, uint8_t speed, uint8_t intensity,
                     uint8_t solidR, uint8_t solidG, uint8_t solidB,
                     uint32_t nowMs);

#endif
