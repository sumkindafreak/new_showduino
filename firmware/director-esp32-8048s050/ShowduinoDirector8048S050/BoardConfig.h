#ifndef SHOWDUINO_BOARD_CONFIG_H
#define SHOWDUINO_BOARD_CONFIG_H

#include <Arduino.h>

// =========================================================
// Showduino Director - Sunton ESP32-S3 RGB panel config
// Same RGB pin map for ESP32-8048S043 and ESP32-8048S050 (800x480 ST7262)
// =========================================================

#define SHOWDUINO_TOUCH_GT911    1
#define SHOWDUINO_TOUCH_XPT2046  1

#define USB_DEBUG_BAUD 115200
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480

#define TFT_BL_PIN 2
#define TFT_BL_ON  HIGH
#define TFT_BL_OFF LOW

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

#define TOUCH_SDA_PIN 19
#define TOUCH_SCL_PIN 20
#define TOUCH_RST_PIN 38
#define TOUCH_INT_PIN 18
#define TOUCH_I2C_ADDR GT911_ADDR1
#define TOUCH_GT911_ROTATION ROTATION_NORMAL

#define TOUCH_CAL_ENABLE       1
#define TOUCH_CAL_RAW_X_LEFT   790
#define TOUCH_CAL_RAW_X_RIGHT   18
#define TOUCH_CAL_RAW_Y_TOP    465
#define TOUCH_CAL_RAW_Y_BOT     25

#define TOUCH_XPT_CS_PIN   38
#define TOUCH_XPT_IRQ_PIN  18
#define TOUCH_XPT_MOSI_PIN 11
#define TOUCH_XPT_MISO_PIN 13
#define TOUCH_XPT_SCK_PIN  12
#define TOUCH_XPT_ROTATION 1
#define TOUCH_XPT_RAW_MIN  200
#define TOUCH_XPT_RAW_MAX  3900

#define SD_CS_PIN   10
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_SCK_PIN  12

#define I2S_DOUT_PIN 17
#define I2S_BCLK_PIN 0
#define I2S_LRC_PIN  18

#define SHOWDUINO_USE_ESPNOW 1
#define SHOWDUINO_USE_UART_FALLBACK 1
#define SHOWDUINO_ESPNOW_CHANNEL 1
#define SHOWDUINO_ESPNOW_MAGIC 0x5348444FUL
#define SHOWDUINO_ESPNOW_VERSION 1
#define SHOWDUINO_ESPNOW_COMMAND_MAX 96

// Replace these six bytes with the built-in C6 station MAC printed by its firmware.
#define SHOWDUINO_P4_C6_MAC_0 0xFF
#define SHOWDUINO_P4_C6_MAC_1 0xFF
#define SHOWDUINO_P4_C6_MAC_2 0xFF
#define SHOWDUINO_P4_C6_MAC_3 0xFF
#define SHOWDUINO_P4_C6_MAC_4 0xFF
#define SHOWDUINO_P4_C6_MAC_5 0xFF

#define STAGE_ENGINE_BAUD   115200
#define STAGE_ENGINE_RX_PIN 44
#define STAGE_ENGINE_TX_PIN 43

#define LVGL_BUFFER_LINES       40
#define HEARTBEAT_INTERVAL_MS   1000UL
#define HELLO_RETRY_INTERVAL_MS 5000UL
#define UI_REFRESH_INTERVAL_MS  250UL

#endif
