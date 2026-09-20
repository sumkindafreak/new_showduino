#ifndef SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H
#define SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H

#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_emergency_node.h"

/*
 * Showduino C3 Emergency Node — specialist ESP-NOW station.
 *
 * GPIO MAP IS UNCONFIRMED. These values are proposed defaults for a Super Mini
 * class ESP32-C3 and must be physically confirmed during breadboard commissioning.
 *
 * Provisional momentary-pushbutton topology:
 *   normally-open momentary pushbutton between GPIO and GND
 *   pinMode INPUT_PULLUP
 *   RELEASED -> HIGH (healthy)
 *   PRESSED  -> LOW  (emergency assertion)
 *
 * The emergency assertion is latched by the Emergency Node state machine.
 * Releasing the pushbutton must not clear the P4 global emergency latch.
 */

#define SHOWDUINO_EMERGENCY_NODE_FW             "0.1.0"
#define SHOWDUINO_EMERGENCY_NODE_BOARD          "ESP32-C3 Super Mini (GPIO UNCONFIRMED)"
#define SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED  0

#define SHOWDUINO_ESPNOW_CHANNEL                1
#define SHOWDUINO_ESTOP_NODE_AP_PASSWORD        "showduino"

/* Provisional momentary emergency pushbutton input — UNCONFIRMED GPIO. */
#define SHOWDUINO_ESTOP_NODE_GPIO               4
#define SHOWDUINO_ESTOP_NODE_OPEN_LEVEL         LOW
#define SHOWDUINO_ESTOP_NODE_PIN_MODE           INPUT_PULLUP

/* Optional local re-arm (BOOT). Never clears P4 emergency. UNCONFIRMED. */
#define SHOWDUINO_ESTOP_NODE_REARM_GPIO         9
#define SHOWDUINO_ESTOP_NODE_REARM_ACTIVE       LOW
#define SHOWDUINO_ESTOP_NODE_REARM_LONG_MS      800UL

/* Optional status LED / NeoPixel. -1 = not fitted. */
#define SHOWDUINO_ESTOP_NODE_LED_GPIO           (-1)
#define SHOWDUINO_ESTOP_NODE_LED_NEOPIXEL       0
#define SHOWDUINO_ESTOP_NODE_LED_ACTIVE         HIGH

/* Optional local buzzer. -1 = not fitted. Non-blocking. Not required for V1. */
#define SHOWDUINO_ESTOP_NODE_BUZZER_GPIO        (-1)
#define SHOWDUINO_ESTOP_NODE_BUZZER_ACTIVE      HIGH

/* Same SSD1306 OLED hardware/pin map as the C3 Pixel Node. */
#define SHOWDUINO_ESTOP_OLED_SDA                 5
#define SHOWDUINO_ESTOP_OLED_SCL                 6
#define SHOWDUINO_ESTOP_OLED_ADDR                0x3C
#define SHOWDUINO_ESTOP_OLED_I2C_HZ              400000UL
#define SHOWDUINO_ESTOP_OLED_WIDTH               128
#define SHOWDUINO_ESTOP_OLED_HEIGHT              64
#define SHOWDUINO_ESTOP_OLED_MARGIN_LEFT         28
#define SHOWDUINO_ESTOP_OLED_MARGIN_TOP          24
#define SHOWDUINO_ESTOP_OLED_VISIBLE_H           38
#define SHOWDUINO_ESTOP_OLED_ROTATION_180        1

#define SHOWDUINO_ESTOP_NODE_INPUT_POLL_MS      5UL

#endif
