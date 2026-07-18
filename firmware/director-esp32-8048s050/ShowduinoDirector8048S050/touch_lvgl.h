#pragma once

#include <TAMC_GT911.h>

/** BankOfDad-style GT911 → LVGL. Pass DISPLAY_ROTATION (0 = landscape). */
void touchLvglInit(TAMC_GT911 &touch, uint16_t width, uint16_t height, uint8_t displayRotation);

/** Soft I2C restore after SD/SPI — never Wire.end(). */
void touchLvglRestoreAfterSd();

bool touchLvglReady();
