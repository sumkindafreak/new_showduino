#ifndef SHOWDUINO_C3_LAMP_NODE_BOARD_CONFIG_H
#define SHOWDUINO_C3_LAMP_NODE_BOARD_CONFIG_H

#include <Adafruit_NeoPixel.h>
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_ownership.h"

/*
 * Showduino C3 Lamp Node — HUNT-proven ESP32-C3 Super Mini OLED board.
 *
 * Board: ESP32-C3 Super Mini with front 0.42" OLED (SSD1306 128x64 controller,
 * visible glass window ~72x40 mapped with HUNT crop). Arduino profile used by
 * HUNT: MakerGO ESP32 C3 SuperMini, else ESP32C3 Dev Module.
 *
 * USB CDC On Boot ENABLED. Flash DIO, 4 MB. Do not assume PSRAM.
 *
 * OLED / button / LED pins copied from HUNT player-node config.h — do not guess.
 * Lamp NeoPixels use GPIO2, the same lamp-data pin the original carbide firmware
 * used on ESP32-C3 Super Mini, and a pin HUNT leaves unused.
 */

#define SHOWDUINO_LAMP_NODE_FW             "0.2.1"
#define SHOWDUINO_LAMP_NODE_BOARD          "ESP32-C3 Super Mini OLED (HUNT)"
#define SHOWDUINO_LAMP_OLED_CONTROLLER     "SSD1306"

#define SHOWDUINO_ESPNOW_CHANNEL           1
#define SHOWDUINO_LAMP_ANNOUNCE_MS         2000UL
#define SHOWDUINO_LAMP_OLED_REFRESH_MS     400UL
#define SHOWDUINO_LAMP_KEY_LONG_MS         650UL
#define SHOWDUINO_LAMP_KEY_DEBOUNCE_MS     45UL

/* HUNT OLED */
#define SHOWDUINO_LAMP_OLED_SDA            5
#define SHOWDUINO_LAMP_OLED_SCL            6
#define SHOWDUINO_LAMP_OLED_ADDR           0x3C
#define SHOWDUINO_LAMP_OLED_I2C_HZ         400000UL
#define SHOWDUINO_LAMP_OLED_WIDTH          128
#define SHOWDUINO_LAMP_OLED_HEIGHT         64
#define SHOWDUINO_LAMP_OLED_MARGIN_LEFT    28
#define SHOWDUINO_LAMP_OLED_MARGIN_RIGHT   4
#define SHOWDUINO_LAMP_OLED_MARGIN_TOP     24
#define SHOWDUINO_LAMP_OLED_VISIBLE_H      38
#define SHOWDUINO_LAMP_OLED_VISIBLE_X      SHOWDUINO_LAMP_OLED_MARGIN_LEFT
#define SHOWDUINO_LAMP_OLED_VISIBLE_Y      SHOWDUINO_LAMP_OLED_MARGIN_TOP
#define SHOWDUINO_LAMP_OLED_ROTATION_180   1
#define SHOWDUINO_LAMP_OLED_SH1106         0

/* HUNT proven buttons (active LOW, internal pull-up) */
#define SHOWDUINO_LAMP_BTN_A               9   /* BOOT / action — local test */
#define SHOWDUINO_LAMP_BTN_B               0   /* menu — OLED page / STATUS */

/* Original carbide lamp: 7 WS2812 on GPIO2. HUNT does not use GPIO2. */
#define SHOWDUINO_LAMP_PIXEL_PIN           2
#define SHOWDUINO_LAMP_PIXEL_COUNT         7
#define SHOWDUINO_LAMP_PIXEL_ORDER         (NEO_GRB + NEO_KHZ800)
#define SHOWDUINO_LAMP_PIXEL_RESISTOR_OHMS 330

/*
 * HUNT also owns these GPIOs. Lamp Node does not drive them:
 *   GPIO3  RGB blue
 *   GPIO4  rumble FET
 *   GPIO7  RGB red
 *   GPIO8  heartbeat LED
 *   GPIO10 buzzer
 * No spare GPIO is taken for an extra WS2812 status pixel — OLED is primary.
 */
#define SHOWDUINO_LAMP_STATUS_PIXEL_PIN    (-1)

#define SHOWDUINO_LAMP_DEFAULT_BRIGHTNESS  80
#define SHOWDUINO_LAMP_AP_PASSWORD         "showduino"
#define SHOWDUINO_LAMP_DISCOVER_MS         SHOWDUINO_OWNER_DISCOVER_MS
#define SHOWDUINO_LAMP_KEEPALIVE_MS        SHOWDUINO_OWNER_KEEPALIVE_MS

#endif
