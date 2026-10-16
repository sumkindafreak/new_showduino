#ifndef SHOWDUINO_LAMP_EFFECTS_H
#define SHOWDUINO_LAMP_EFFECTS_H

#include <Arduino.h>
#include "../BoardConfig.h"
#define SHOWDUINO_LED_GPIO SHOWDUINO_LAMP_PIXEL_PIN
#include "../../../shared-pixel/ShowduinoLedStrip.h"
#include "../../../protocol/showduino_lamp_node.h"

void lampEffectsReset(ShowduinoLampFx fx);
void lampEffectsTick(ShowduinoLedStrip &strip, ShowduinoLampFx fx,
                     uint8_t bri0to100, uint8_t speed, uint8_t intensity,
                     uint8_t solidR, uint8_t solidG, uint8_t solidB,
                     uint32_t nowMs);

#endif
