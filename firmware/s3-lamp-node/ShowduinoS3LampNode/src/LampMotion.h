#ifndef SHOWDUINO_S3_LAMP_MOTION_H
#define SHOWDUINO_S3_LAMP_MOTION_H

#include "../../../protocol/showduino_lamp_motion.h"

void lampMotionBegin();
void lampMotionService();
void lampMotionApplyConfig();
ShowduinoMotionEvent lampMotionTakeEvent();
const ShowduinoMotionDetector *lampMotionDetector();
int lampMotionPin();
bool lampMotionPinAssigned();
bool lampMotionHardwareConfirmed();
uint8_t lampMotionRawHigh();
uint8_t lampMotionActive();
const char *lampMotionRawName();
const char *lampMotionStateName();
const char *lampMotionPhysicalStatus();

#endif
