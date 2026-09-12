#ifndef SHOWDUINO_S3_LAMP_CONFIG_H
#define SHOWDUINO_S3_LAMP_CONFIG_H

#include <stddef.h>
#include "../../../protocol/showduino_lamp_node.h"

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

#endif
