#pragma once

#include "TouchCalibrationMath.h"
#include <stddef.h>
#include <stdint.h>

/** Director NVS namespace is 15 chars: showduino_touch. Key: cal. */
bool touchCalibrationStoreLoad(ShowduinoTouchCalibrationRecord *out, int *failCode);
bool touchCalibrationStoreSave(const ShowduinoTouchCalibrationRecord *rec);
bool touchCalibrationStoreErase();
