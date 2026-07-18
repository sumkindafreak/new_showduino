#pragma once

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "BoardConfig.h"

/** Landscape: native 800x480, no 90° rotation. */
#define DISPLAY_ROTATION 0

/** RGB DMA bounce buffer — required for stable ESP32-S3 RGB panels (BankOfDad bring-up). */
#define RGB_BOUNCE_BUFFER (SCREEN_WIDTH * 20)

extern Arduino_RGB_Display *gfx;

bool lvglPortInit(Arduino_RGB_Display *panel, Arduino_ESP32RGBPanel *rgbPanel);
void lvglPortLoop();
