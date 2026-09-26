#ifndef SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H
#define SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H

#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_emergency_node.h"

/*
 * Showduino C3 Emergency Node — specialist ESP-NOW station.
 *
 * Physical control: momentary NO pushbutton to GND on GPIO4, INPUT_PULLUP.
 *   RELEASED = HIGH
 *   PRESSED  = LOW  → assert / latch emergency
 * Button release never clears P4 or the local latch.
 *
 * GPIO4 button map remains physically UNVERIFIED until bench commissioning.
 * Do not set SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED until confirmed.
 *
 * OLED: same SSD1306 geometry as the C3 Pixel Node (SDA5/SCL6).
 */

#define SHOWDUINO_EMERGENCY_NODE_FW             "0.2.0"
#define SHOWDUINO_EMERGENCY_NODE_BOARD          "ESP32-C3 Super Mini OLED"
#define SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED  0

#define SHOWDUINO_ESPNOW_CHANNEL                1
#define SHOWDUINO_ESTOP_NODE_AP_PASSWORD        "showduino"
#define SHOWDUINO_ESTOP_NODE_OLED_REFRESH_MS    400UL

/* Momentary emergency pushbutton — intended map, physically unverified. */
#define SHOWDUINO_ESTOP_NODE_GPIO               4
#define SHOWDUINO_ESTOP_NODE_ACTIVE_LEVEL       LOW
#define SHOWDUINO_ESTOP_NODE_PIN_MODE           INPUT_PULLUP

/* Maintenance-only local reset (BOOT). Never clears P4. Not required for
 * normal use after global clear with button released. */
#define SHOWDUINO_ESTOP_NODE_REARM_GPIO         9
#define SHOWDUINO_ESTOP_NODE_REARM_ACTIVE       LOW
#define SHOWDUINO_ESTOP_NODE_REARM_LONG_MS      800UL

/* Optional status LED / NeoPixel. -1 = not fitted. */
#define SHOWDUINO_ESTOP_NODE_LED_GPIO           (-1)
#define SHOWDUINO_ESTOP_NODE_LED_NEOPIXEL       0
#define SHOWDUINO_ESTOP_NODE_LED_ACTIVE         HIGH

/* Optional local buzzer. -1 = not fitted. Non-blocking. */
#define SHOWDUINO_ESTOP_NODE_BUZZER_GPIO        (-1)
#define SHOWDUINO_ESTOP_NODE_BUZZER_ACTIVE      HIGH

#define SHOWDUINO_ESTOP_NODE_INPUT_POLL_MS      5UL

/* Locked OLED — copy of C3 Pixel Node / HUNT geometry. Do not change. */
#define OLED_SDA_PIN                            5
#define OLED_SCL_PIN                            6
#define OLED_I2C_ADDRESS                        0x3C
#define OLED_I2C_HZ                             400000UL
#define OLED_WIDTH                              128
#define OLED_HEIGHT                             64
#define OLED_PANEL_MARGIN_LEFT                  28
#define OLED_PANEL_MARGIN_RIGHT                 4
#define OLED_PANEL_MARGIN_TOP                   24
#define OLED_PANEL_VISIBLE_H                    38
#define OLED_VISIBLE_X                          OLED_PANEL_MARGIN_LEFT
#define OLED_VISIBLE_Y                          OLED_PANEL_MARGIN_TOP
#define OLED_ROTATION_180                       true
#define OLED_DRIVER_SH1106                      false

#define SHOWDUINO_ESTOP_OLED_CONTROLLER         "SSD1306"
#define SHOWDUINO_ESTOP_OLED_SDA                OLED_SDA_PIN
#define SHOWDUINO_ESTOP_OLED_SCL                OLED_SCL_PIN
#define SHOWDUINO_ESTOP_OLED_ADDR               OLED_I2C_ADDRESS
#define SHOWDUINO_ESTOP_OLED_I2C_HZ             OLED_I2C_HZ
#define SHOWDUINO_ESTOP_OLED_WIDTH              OLED_WIDTH
#define SHOWDUINO_ESTOP_OLED_HEIGHT             OLED_HEIGHT
#define SHOWDUINO_ESTOP_OLED_VISIBLE_X          OLED_VISIBLE_X
#define SHOWDUINO_ESTOP_OLED_VISIBLE_Y          OLED_VISIBLE_Y
#define SHOWDUINO_ESTOP_OLED_VISIBLE_H          OLED_PANEL_VISIBLE_H
#define SHOWDUINO_ESTOP_OLED_ROTATION_180       1

#endif
