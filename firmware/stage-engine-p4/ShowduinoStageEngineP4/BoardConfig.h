#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>
#include "../../../protocol/showduino_protocol_version.h"

#define SHOWDUINO_BOARD_NAME "ESP32-P4"
#define STAGE_FW_VERSION     "0.9.0-stage"

#define SHOWDUINO_WEBUI_ENABLED 1

// =========================================================
// Stage Controller microSD (SPI)
// Defaults match Espressif / Waveshare ESP32-P4 Function EV
// onboard microSD. Change these if you use a different board
// or an external SPI SD module.
// Link UART stays on GPIO5/6 — do not reuse those for SD.
// =========================================================
#define SHOWDUINO_SD_ENABLED     1
#define SHOWDUINO_SD_SCK_PIN     43
#define SHOWDUINO_SD_MISO_PIN    39
#define SHOWDUINO_SD_MOSI_PIN    44
#define SHOWDUINO_SD_CS_PIN      42
/* Set to -1 if your board has no SD power-enable GPIO. */
#define SHOWDUINO_SD_POWER_PIN   45
#define SHOWDUINO_SD_POWER_ON_LEVEL LOW
#define SHOWDUINO_SD_SPI_HZ      10000000UL

#define PATH_SHOWDUINO_ROOT      "/showduino"
#define PATH_WEBUI_WWW           "/showduino/www"
#define PATH_SHOW_PACKAGES       "/showduino/shows/packages"
#define PATH_SHOW_INDEX          "/showduino/shows/index.json"

// =========================================================
// Emergency Neopixel line (local Stage Controller indicator)
// One strip turns solid white on EMERGENCY:STOP / E-stop.
// Blackout on CLEAR / Abort Show.
// Change PIN / COUNT to match your wiring. Avoid UART 5/6
// and SD pins 39/42/43/44/45.
// Requires library: Adafruit NeoPixel
// =========================================================
#define SHOWDUINO_EMERGENCY_PIXEL_ENABLED     1
#define SHOWDUINO_EMERGENCY_PIXEL_PIN         21
#define SHOWDUINO_EMERGENCY_PIXEL_COUNT       30
#define SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS  255

#endif
