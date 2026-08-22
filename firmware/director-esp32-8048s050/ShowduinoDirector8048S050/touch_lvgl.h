#pragma once

#include <TAMC_GT911.h>
#include <stdint.h>

/**
 * GT911 → LVGL (BankOfDad pin map + landscape calibration from SdTouchTest).
 * displayRotation is ignored for TAMC_GT911 — library rotation enums ≠ GFX rotation.
 */
void touchLvglInit(TAMC_GT911 &touch, uint16_t width, uint16_t height, uint8_t displayRotation);

/** Full GT911 re-init after SD/SPI — never Wire.end(). */
void touchLvglRestoreAfterSd();

bool touchLvglReady();

/** Direct GT911 poll while backlight is off (wake without relying on LVGL). */
bool touchLvglPollActivity();

/** Optional Phase-2 DisplayManager touch feed (coords already mapped to panel space). */
typedef void (*TouchLvglHook)(int32_t x, int32_t y, bool pressed);
void touchLvglSetHook(TouchLvglHook hook);