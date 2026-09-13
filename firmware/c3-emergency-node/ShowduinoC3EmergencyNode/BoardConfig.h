#ifndef SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H
#define SHOWDUINO_C3_EMERGENCY_NODE_BOARD_CONFIG_H

#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_emergency_node.h"

/*
 * Showduino C3 Emergency Node — specialist ESP-NOW station.
 *
 * GPIO MAP IS UNCONFIRMED. These values are proposed defaults for a Super Mini
 * class ESP32-C3. Do not treat them as physically verified. Do not flash this
 * firmware until the mushroom switch wiring is commissioned on the bench.
 *
 * Proposed NC topology (preferred):
 *   mushroom NC contact between GPIO and GND
 *   pinMode INPUT_PULLUP
 *   CLOSED (healthy)  -> LOW
 *   OPEN (pressed or local wire broken) -> HIGH -> EMERGENCY CONDITION
 *
 * Opening the local NC input is an emergency whether caused by a button press
 * or a broken local conductor. V1 does not distinguish those two cases.
 */

#define SHOWDUINO_EMERGENCY_NODE_FW             "0.1.0"
#define SHOWDUINO_EMERGENCY_NODE_BOARD          "ESP32-C3 Super Mini (GPIO UNCONFIRMED)"
#define SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED  0

#define SHOWDUINO_ESPNOW_CHANNEL                1
#define SHOWDUINO_ESTOP_NODE_AP_PASSWORD        "showduino"

/* Proposed NC input — UNCONFIRMED. */
#define SHOWDUINO_ESTOP_NODE_GPIO               4
#define SHOWDUINO_ESTOP_NODE_OPEN_LEVEL         HIGH
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

#define SHOWDUINO_ESTOP_NODE_INPUT_POLL_MS      5UL

#endif
