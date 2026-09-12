#ifndef SHOWDUINO_S3_LAMP_SENSORS_H
#define SHOWDUINO_S3_LAMP_SENSORS_H

#include "../../../protocol/showduino_carbide_lamp.h"

void lampSensorsBegin();
void lampSensorsService();
const ShowduinoBlowDetector *lampSensorsBlow();
ShowduinoBlowClass lampSensorsTakeBlowEvent();
int32_t lampSensorsMicRaw();
int32_t lampSensorsMicFiltered();
int32_t lampSensorsMicBaseline();
int32_t lampSensorsLightRaw();
int32_t lampSensorsLightFiltered();
int32_t lampSensorsLightNormalized(); /* -1 if uncalibrated */
void lampSensorsApplyConfig();
void lampSensorsCalibrateQuiet();
void lampSensorsCalibrateLight();
int32_t lampSensorsVoltRaw();
int32_t lampSensorsVoltFiltered();
int32_t lampSensorsVoltMv(); /* -1 if scale missing; default is 5.00 V FS */
bool lampSensorsCalibrateVoltFullScale(); /* store current ADC as 5.00 V */
const char *lampSensorsMicStatus();
const char *lampSensorsLightStatus();
const char *lampSensorsVoltStatus();
const char *lampSensorsVoltWarn();

#endif
