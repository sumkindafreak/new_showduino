#ifndef SHOWDUINO_BOARD_CONFIG_H
#define SHOWDUINO_BOARD_CONFIG_H

#include <Arduino.h>

// =========================================================
// Showduino Director - Sunton ESP32-S3 RGB panel config
// Same RGB pin map for ESP32-8048S043 and ESP32-8048S050 (800x480 ST7262)
// =========================================================

// Touch: capacitive GT911 first. Keep XPT2046 off unless you have the resistive (R) panel —
// its CS pin is GPIO38, same as GT911 RST, and probing it can kill capacitive touch.
#define SHOWDUINO_TOUCH_GT911    1
#define SHOWDUINO_TOUCH_XPT2046  0

// USB debug serial
#define USB_DEBUG_BAUD 115200

// Screen size
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

// Backlight
#define TFT_BL_PIN 2
#define TFT_BL_ON  HIGH
#define TFT_BL_OFF LOW

// RGB display timing for ST7262 IPS LCD 800x480
#define RGB_DE_PIN    40
#define RGB_VSYNC_PIN 41
#define RGB_HSYNC_PIN 39
#define RGB_PCLK_PIN  42

#define RGB_R0_PIN 45
#define RGB_R1_PIN 48
#define RGB_R2_PIN 47
#define RGB_R3_PIN 21
#define RGB_R4_PIN 14

#define RGB_G0_PIN 5
#define RGB_G1_PIN 6
#define RGB_G2_PIN 7
#define RGB_G3_PIN 15
#define RGB_G4_PIN 16
#define RGB_G5_PIN 4

#define RGB_B0_PIN 8
#define RGB_B1_PIN 3
#define RGB_B2_PIN 46
#define RGB_B3_PIN 9
#define RGB_B4_PIN 1

#define RGB_HSYNC_POLARITY 0
#define RGB_HSYNC_FRONT    8
#define RGB_HSYNC_PULSE    4
#define RGB_HSYNC_BACK     8
#define RGB_VSYNC_POLARITY 0
#define RGB_VSYNC_FRONT    8
#define RGB_VSYNC_PULSE    4
#define RGB_VSYNC_BACK     8
#define RGB_PCLK_ACTIVE_NEG 1
#define RGB_PREFER_SPEED   16000000

// GT911 capacitive touch (JC8048W550C / 8048S050C) — BankOfDad pin map
#define TOUCH_SDA_PIN 19
#define TOUCH_SCL_PIN 20
#define TOUCH_INT_PIN 18
#define TOUCH_RST_PIN 38
// Landscape: no axis remap in touch_lvgl (DISPLAY_ROTATION 0).
#define TOUCH_GT911_ROTATION 0

// XPT2046 resistive (legacy R panels) — not used with new capacitive bring-up
#define TOUCH_XPT_CS_PIN   38
#define TOUCH_XPT_IRQ_PIN  18
#define TOUCH_XPT_MOSI_PIN 11
#define TOUCH_XPT_MISO_PIN 13
#define TOUCH_XPT_SCK_PIN  12
#define TOUCH_XPT_ROTATION 1
#define TOUCH_XPT_RAW_MIN  200
#define TOUCH_XPT_RAW_MAX  3900

// SD card SPI from the SDK
#define SD_CS_PIN   10
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_SCK_PIN  12

// I2S audio pins from the SDK. GPIO17/18 are now reserved for Stage UART,
// so on-board I2S is deferred until alternate pins are assigned.
#define I2S_DOUT_PIN 17
#define I2S_BCLK_PIN 0
#define I2S_LRC_PIN  18

// =========================================================
// Portable controller transport
// =========================================================
#define SHOWDUINO_USE_ESPNOW 1
// Keep off while C3 owns P4 UART pins 5/6 (also avoids GT911 INT conflict on GPIO18).
#define SHOWDUINO_USE_UART_FALLBACK 0
#define SHOWDUINO_ESPNOW_CHANNEL 1
/* Magic / wire version / command max: protocol/showduino_protocol_version.h */
#include "../../../protocol/showduino_protocol_version.h"

// C3 SuperMini ESP-NOW bridge peer MAC: 88:56:A6:6E:80:0C
#define SHOWDUINO_P4_C6_MAC_0 0x88
#define SHOWDUINO_P4_C6_MAC_1 0x56
#define SHOWDUINO_P4_C6_MAC_2 0xA6
#define SHOWDUINO_P4_C6_MAC_3 0x6E
#define SHOWDUINO_P4_C6_MAC_4 0x80
#define SHOWDUINO_P4_C6_MAC_5 0x0C

// Optional direct UART fallback to P4 (only if C3 is NOT using P4 pins 5/6).
// Preferred wireless path: Director --ESP-NOW--> C3 --UART--> P4 GPIO5/6
#define STAGE_ENGINE_BAUD   115200
#define STAGE_ENGINE_RX_PIN 18
#define STAGE_ENGINE_TX_PIN 17

#define LVGL_BUFFER_LINES       40
#define HEARTBEAT_INTERVAL_MS   2000UL
#define HELLO_RETRY_INTERVAL_MS  2000UL
#define UI_REFRESH_INTERVAL_MS  1000UL
// Sustained silence before DISCONNECTED (~3 missed heartbeats).
#define LINK_TIMEOUT_MS         7000UL
#define ESPNOW_RECOVER_MS       5000UL

enum ShowduinoLinkState : uint8_t {
  LINK_SEARCHING = 0,
  LINK_READY = 1,
  LINK_DISCONNECTED = 2
};

#endif
