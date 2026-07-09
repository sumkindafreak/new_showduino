#ifndef SHOWDUINO_BOARD_CONFIG_H
#define SHOWDUINO_BOARD_CONFIG_H

#include <Arduino.h>

// =========================================================
// Showduino Director - ESP32-8048S050 5.0" RGB board config
// Manufacturer SDK source: 5.0inch_ESP32-8048S050 demo package
// =========================================================

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

// GT911 capacitive touch from the SDK
#define TOUCH_SDA_PIN 19
#define TOUCH_SCL_PIN 20
#define TOUCH_INT_PIN -1
#define TOUCH_RST_PIN 38
#define TOUCH_MAP_X1  800
#define TOUCH_MAP_X2  0
#define TOUCH_MAP_Y1  480
#define TOUCH_MAP_Y2  0

// SD card SPI from the SDK
#define SD_CS_PIN   10
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_SCK_PIN  12

// I2S audio pins from the SDK. Not used in this base yet, but documented ready.
#define I2S_DOUT_PIN 17
#define I2S_BCLK_PIN 0
#define I2S_LRC_PIN  18

// =========================================================
// Portable controller transport
// =========================================================
// Primary live link: ESP-NOW from the 5" ESP32-S3 Director to the P4 board's built-in ESP32-C6.
// The C6 should receive these packets and forward the command to the P4 internally.
#define SHOWDUINO_USE_ESPNOW 1
#define SHOWDUINO_USE_UART_FALLBACK 1
#define SHOWDUINO_ESPNOW_CHANNEL 1
#define SHOWDUINO_ESPNOW_MAGIC 0x5348444FUL  // "SHDO"
#define SHOWDUINO_ESPNOW_VERSION 1
#define SHOWDUINO_ESPNOW_COMMAND_MAX 96

// Replace this with the MAC address of the P4 board's built-in ESP32-C6 bridge.
// Until confirmed, this is a broadcast-style placeholder and must be changed for reliable pairing.
#define SHOWDUINO_P4_C6_MAC_0 0xFF
#define SHOWDUINO_P4_C6_MAC_1 0xFF
#define SHOWDUINO_P4_C6_MAC_2 0xFF
#define SHOWDUINO_P4_C6_MAC_3 0xFF
#define SHOWDUINO_P4_C6_MAC_4 0xFF
#define SHOWDUINO_P4_C6_MAC_5 0xFF

// Service UART fallback. IMPORTANT: 19/20 are used by GT911 touch, so we do NOT use them here.
// GPIO43/44 are left free by the LCD/touch/SD pins on this board and work well for a bench UART link.
#define STAGE_ENGINE_BAUD   115200
#define STAGE_ENGINE_RX_PIN 44
#define STAGE_ENGINE_TX_PIN 43

// UI / system timing
#define LVGL_BUFFER_LINES       40
#define HEARTBEAT_INTERVAL_MS   1000UL
#define HELLO_RETRY_INTERVAL_MS 5000UL
#define UI_REFRESH_INTERVAL_MS  250UL

#endif
