#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>
#include "../../../protocol/showduino_protocol_version.h"

#define SHOWDUINO_BOARD_NAME "ESP32-P4"
#define STAGE_FW_VERSION     "0.9.0-stage"

#define SHOWDUINO_WEBUI_ENABLED 1

// =========================================================
// Link UART to Communications Engine (C3)
// Canonical: C3 TX21 -> P4 RX, C3 RX20 <- P4 TX
// Do not reuse these GPIOs for SD or I2S.
// =========================================================
#define SHOWDUINO_LINK_UART_BAUD  115200
#define SHOWDUINO_LINK_RX_PIN     5
#define SHOWDUINO_LINK_TX_PIN     6

/* Legacy aliases used by existing Stage sketch */
#ifndef LINK_RX_PIN
#define LINK_RX_PIN SHOWDUINO_LINK_RX_PIN
#endif
#ifndef LINK_TX_PIN
#define LINK_TX_PIN SHOWDUINO_LINK_TX_PIN
#endif

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
// Local P4 audio — ONE output only (IAN main/local channel)
// Remote zone audio is ESP-NOW command -> audio nodes (own ESP32+I2S+SD).
// Do NOT stream PCM/WAV over ESP-NOW.
//
// Repository audit (2026-07): no Waveshare P4 I2S pin map is defined for
// Stage Engine. SUE docs use BCLK=5/WS=6/DOUT=7 which CONFLICT with link UART.
// Pins remain unassigned (-1) until hardware confirmation — do not invent.
// =========================================================
#define SHOWDUINO_P4_LOCAL_AUDIO_ENABLED  0
#define P4_AUDIO_I2S_BCLK                 (-1)
#define P4_AUDIO_I2S_WS                   (-1)
#define P4_AUDIO_I2S_DOUT                 (-1)
/* Optional aliases */
#define SHOWDUINO_P4_I2S_BCLK_PIN         P4_AUDIO_I2S_BCLK
#define SHOWDUINO_P4_I2S_WS_PIN           P4_AUDIO_I2S_WS
#define SHOWDUINO_P4_I2S_DOUT_PIN         P4_AUDIO_I2S_DOUT

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