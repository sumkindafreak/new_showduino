#ifndef SHOWDUINO_C3_PIXEL_NODE_BOARD_CONFIG_H
#define SHOWDUINO_C3_PIXEL_NODE_BOARD_CONFIG_H

#include <Adafruit_NeoPixel.h>
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_ownership.h"
#include "../../../protocol/showduino_pixel_node.h"

/*
 * Showduino C3 Pixel Node — same HUNT-proven ESP32-C3 Super Mini OLED board
 * as the Lamp Node.
 *
 * OLED pins are locked hardware. Do not move them to free a pixel GPIO.
 *
 * Pixel DATA GPIO: GPIO2
 *   - Lamp Node already drives 7× WS2812 on GPIO2 on this board.
 *   - HUNT player-node leaves GPIO2 unused (RGB 3/7, rumble 4, heartbeat 8,
 *     buzzer 10, BOOT 9, Button B 0).
 *   - GPIO2 is not a strapping pin on ESP32-C3 Super Mini.
 *   - USB CDC remains on the native USB pins; GPIO2 does not conflict.
 *   - Reliable addressable-pixel output is already proven on this pin.
 *
 * C3 max pixels is 512 (not the P4's 1024). WS2812 bit time is ~1.25 µs × 24 × N:
 *   512 ≈ 15.4 ms, which still fits a 20 ms frame plus ESP-NOW/OLED/Wi-Fi.
 *   1024 ≈ 30.7 ms and would starve the radio/watchdog on this MCU.
 * Logical Showduino model is identical; only the hardware maximum differs.
 */

#define SHOWDUINO_PIXEL_NODE_FW             "0.1.0"
#define SHOWDUINO_PIXEL_NODE_BOARD          "ESP32-C3 Super Mini OLED (HUNT)"
#define SHOWDUINO_PIXEL_OLED_CONTROLLER     "SSD1306"

#define SHOWDUINO_ESPNOW_CHANNEL            1
#define SHOWDUINO_PIXEL_ANNOUNCE_MS         2000UL
#define SHOWDUINO_PIXEL_ANNOUNCE_SEARCH_MS  1500UL
#define SHOWDUINO_PIXEL_OLED_REFRESH_MS     400UL
#define SHOWDUINO_PIXEL_KEY_LONG_MS         650UL
#define SHOWDUINO_PIXEL_KEY_DEBOUNCE_MS     45UL

/* Locked OLED — copy of Lamp / HUNT geometry. Do not change. */
#define OLED_SDA_PIN                        5
#define OLED_SCL_PIN                        6
#define OLED_I2C_ADDRESS                    0x3C
#define OLED_I2C_HZ                         400000UL
#define OLED_WIDTH                          128
#define OLED_HEIGHT                         64
#define OLED_PANEL_MARGIN_LEFT              28
#define OLED_PANEL_MARGIN_RIGHT             4
#define OLED_PANEL_MARGIN_TOP               24
#define OLED_PANEL_VISIBLE_H                38
#define OLED_VISIBLE_X                      OLED_PANEL_MARGIN_LEFT
#define OLED_VISIBLE_Y                      OLED_PANEL_MARGIN_TOP
#define OLED_ROTATION_180                   true
#define OLED_DRIVER_SH1106                  false

#define SHOWDUINO_PIXEL_OLED_SDA            OLED_SDA_PIN
#define SHOWDUINO_PIXEL_OLED_SCL            OLED_SCL_PIN
#define SHOWDUINO_PIXEL_OLED_ADDR           OLED_I2C_ADDRESS
#define SHOWDUINO_PIXEL_OLED_I2C_HZ         OLED_I2C_HZ
#define SHOWDUINO_PIXEL_OLED_WIDTH          OLED_WIDTH
#define SHOWDUINO_PIXEL_OLED_HEIGHT         OLED_HEIGHT
#define SHOWDUINO_PIXEL_OLED_MARGIN_LEFT    OLED_PANEL_MARGIN_LEFT
#define SHOWDUINO_PIXEL_OLED_MARGIN_RIGHT   OLED_PANEL_MARGIN_RIGHT
#define SHOWDUINO_PIXEL_OLED_MARGIN_TOP     OLED_PANEL_MARGIN_TOP
#define SHOWDUINO_PIXEL_OLED_VISIBLE_H      OLED_PANEL_VISIBLE_H
#define SHOWDUINO_PIXEL_OLED_VISIBLE_X      OLED_VISIBLE_X
#define SHOWDUINO_PIXEL_OLED_VISIBLE_Y      OLED_VISIBLE_Y
#define SHOWDUINO_PIXEL_OLED_ROTATION_180   1
#define SHOWDUINO_PIXEL_OLED_SH1106         0

#define SHOWDUINO_PIXEL_BTN_A               9   /* BOOT / local test */
#define SHOWDUINO_PIXEL_BTN_B               0   /* OLED page / STATUS */

#define SHOWDUINO_PIXEL_DATA_PIN            2
#define SHOWDUINO_PIXEL_ORDER               (NEO_GRB + NEO_KHZ800)
#define SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS  330

#define SHOWDUINO_PIXEL_AP_PASSWORD         "showduino"
#define SHOWDUINO_PIXEL_DISCOVER_MS         SHOWDUINO_OWNER_DISCOVER_MS
#define SHOWDUINO_PIXEL_KEEPALIVE_MS        SHOWDUINO_OWNER_KEEPALIVE_MS

#endif
