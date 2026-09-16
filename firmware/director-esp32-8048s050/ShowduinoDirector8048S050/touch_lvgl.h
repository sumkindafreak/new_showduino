#pragma once

#include <TAMC_GT911.h>
#include <stdint.h>
#include "TouchCalibrationMath.h"

struct TouchRawPoint {
  int32_t x;
  int32_t y;
};

/**
 * GT911 -> LVGL (BankOfDad pin map + landscape calibration from SdTouchTest).
 * displayRotation is ignored for TAMC_GT911 - library rotation enums ≠ GFX rotation.
 *
 * Mapping is either NVS affine calibration or the factory min/max fallback.
 * Never both.
 */
void touchLvglInit(TAMC_GT911 &touch, uint16_t width, uint16_t height, uint8_t displayRotation);

/** Full GT911 re-init after SD/SPI - never Wire.end(). */
void touchLvglRestoreAfterSd();

bool touchLvglReady();

/** Direct GT911 poll while backlight is off (wake without relying on LVGL). */
bool touchLvglPollActivity();

/** Optional Phase-2 DisplayManager touch feed (coords already mapped to panel space).
 *  Return true to consume this press/release cycle so LVGL does not see it. */
typedef bool (*TouchLvglHook)(int32_t x, int32_t y, bool pressed);
void touchLvglSetHook(TouchLvglHook hook);
void touchLvglConsumeUntilRelease();

bool touchLvglReadRaw(TouchRawPoint &point);
ShowduinoTouchCalMode touchLvglCalibrationMode();
bool touchLvglCalibrationIsNvs();
uint16_t touchLvglCalibrationVersion();
void touchLvglPrintCalibrationStatus();
bool touchLvglSaveCalibration(const ShowduinoTouchCalibrationRecord &rec);
bool touchLvglResetCalibration();
void touchLvglMapRaw(int32_t rawX, int32_t rawY, int32_t *screenX, int32_t *screenY);
void touchLvglMapWith(const ShowduinoTouchCalibrationRecord *rec,
                      int32_t rawX, int32_t rawY, int32_t *screenX, int32_t *screenY);
