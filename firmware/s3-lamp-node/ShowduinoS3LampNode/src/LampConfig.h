#ifndef SHOWDUINO_S3_LAMP_CONFIG_H
#define SHOWDUINO_S3_LAMP_CONFIG_H

#include <stddef.h>
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_lamp_motion.h"

void lampConfigBegin();
const char *lampConfigId();
const char *lampConfigName();
bool lampConfigSetId(const char *id);
bool lampConfigSetName(const char *name);
uint8_t lampConfigBrightness();
void lampConfigSetBrightness(uint8_t bri);
uint8_t lampConfigAudioVolume();
void lampConfigSetAudioVolume(uint8_t vol);
int32_t lampConfigBlowThreshold();
void lampConfigSetBlowThreshold(int32_t v);
uint16_t lampConfigPuffMs();
uint16_t lampConfigBlowMs();
void lampConfigSetBlowWindows(uint16_t puffMs, uint16_t blowMs);
uint32_t lampConfigVoltScaleNum();
uint32_t lampConfigVoltScaleDen();
void lampConfigSetVoltScale(uint32_t num, uint32_t den);
uint32_t lampConfigVoltWarnMv();
uint32_t lampConfigVoltUnderMv();
uint32_t lampConfigLightScale();
void lampConfigSetLightScale(uint32_t scale);
uint8_t lampConfigMotionEnabled();
void lampConfigSetMotionEnabled(uint8_t en);
ShowduinoMotionAction lampConfigMotionAction();
void lampConfigSetMotionAction(ShowduinoMotionAction act);
uint8_t lampConfigMotionActiveLow();
void lampConfigSetMotionActiveLow(uint8_t activeLow);
uint16_t lampConfigMotionCooldownMs();
void lampConfigSetMotionCooldownMs(uint16_t ms);
uint8_t lampConfigFlameActivity();
void lampConfigSetFlameActivity(uint8_t v);
uint8_t lampConfigFlickerAmount();
void lampConfigSetFlickerAmount(uint8_t v);
uint8_t lampConfigIgnitionSpeed();
void lampConfigSetIgnitionSpeed(uint8_t v);
uint8_t lampConfigJewelCore();
void lampConfigSetJewelCore(uint8_t v);

#endif
