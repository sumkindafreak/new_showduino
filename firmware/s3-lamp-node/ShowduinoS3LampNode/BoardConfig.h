#ifndef SHOWDUINO_S3_LAMP_NODE_BOARD_CONFIG_H
#define SHOWDUINO_S3_LAMP_NODE_BOARD_CONFIG_H

#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_ownership.h"
#include "../../../protocol/showduino_version.h"

/*
 * Showduino S3 Lamp Node — ESP32-S3 interactive carbide-lamp simulator.
 *
 * MCU CONFIRMED FROM PHYSICAL AUDIT: ESP32-S3, same development-board
 * family as the Showduino S3 Communications Controller.
 *
 * GPIOs are NOT confirmed. Do not treat any pin number in this file as
 * production wiring. Flash is forbidden until Toby traces the jewel,
 * button, sensors, and Fermion UART.
 *
 * SHOWDUINO_LAMP_PINS_CONFIRMED must stay 0 until every required GPIO
 * below is physically verified. Compile-only placeholder pins, if ever
 * enabled, are isolated behind SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS and
 * must never be mistaken for confirmed production wiring.
 */

#define SHOWDUINO_LAMP_NODE_FW             "0.3.0"
#define SHOWDUINO_LAMP_NODE_BOARD          "ESP32-S3 Dev Module (Lamp Node)"
#define SHOWDUINO_LAMP_NODE_TARGET         "S3"

#ifndef SHOWDUINO_LAMP_PINS_CONFIRMED
#define SHOWDUINO_LAMP_PINS_CONFIRMED      0
#endif

#ifndef SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS
#define SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS 0
#endif

#if SHOWDUINO_LAMP_PINS_CONFIRMED && SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS
#error "Do not enable confirmed pins and development placeholders together"
#endif

#define SHOWDUINO_ESPNOW_CHANNEL           1
#define SHOWDUINO_LAMP_ANNOUNCE_MS         2000UL
#define SHOWDUINO_LAMP_ANNOUNCE_SEARCH_MS  1500UL
#define SHOWDUINO_LAMP_KEY_LONG_MS         650UL
#define SHOWDUINO_LAMP_KEY_DEBOUNCE_MS     45UL
#define SHOWDUINO_LAMP_PIXEL_COUNT         7
#define SHOWDUINO_LAMP_DEFAULT_BRIGHTNESS  80
#define SHOWDUINO_LAMP_AP_PASSWORD         "showduino"
#define SHOWDUINO_LAMP_DISCOVER_MS         SHOWDUINO_OWNER_DISCOVER_MS
#define SHOWDUINO_LAMP_KEEPALIVE_MS        SHOWDUINO_OWNER_KEEPALIVE_MS
#define SHOWDUINO_LAMP_SENSOR_MIC_MS       20UL
#define SHOWDUINO_LAMP_SENSOR_LIGHT_MS     200UL
#define SHOWDUINO_LAMP_SENSOR_VOLT_MS      500UL
#define SHOWDUINO_LAMP_FERMION_BAUD        115200UL

/*
 * ADC note for pin selection (PHYSICAL CONFIRMATION REQUIRED):
 * Mic, light, and voltage need three analog inputs. On ESP32-S3, use ADC1
 * (typically GPIO1–GPIO10). Wi-Fi / ESP-NOW conflicts with ADC2.
 * Avoid USB GPIO19/20, UART0 GPIO43/44, and strapping GPIO0/3/45/46.
 */

#if SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS
/* DEVELOPMENT ONLY — not production wiring. Isolated so firmware can be
 * exercised on a spare S3 without implying the physical Lamp Node is mapped. */
#define SHOWDUINO_LAMP_PIXEL_PIN           2
#define SHOWDUINO_LAMP_BTN_IGNITE          9
#define SHOWDUINO_LAMP_MIC_PIN             1
#define SHOWDUINO_LAMP_LIGHT_PIN           4
#define SHOWDUINO_LAMP_VOLT_PIN            5
#define SHOWDUINO_LAMP_FERMION_TX_PIN      17
#define SHOWDUINO_LAMP_FERMION_RX_PIN      18
#define SHOWDUINO_LAMP_PIN_SOURCE          "DEV_PLACEHOLDER"
#else
#define SHOWDUINO_LAMP_PIXEL_PIN           (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_BTN_IGNITE          (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_MIC_PIN             (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_LIGHT_PIN           (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_VOLT_PIN            (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_FERMION_TX_PIN      (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_FERMION_RX_PIN      (-1)  /* PHYSICAL CONFIRMATION REQUIRED */
#define SHOWDUINO_LAMP_PIN_SOURCE          "UNCONFIRMED"
#endif

/* Button polarity is unknown until traced. Firmware assumes active-LOW
 * with internal pull-up, the usual ESP32 momentary-to-GND convention.
 * Confirm before treating a missed press as a firmware bug. */
#define SHOWDUINO_LAMP_BTN_ACTIVE_LOW      1

#define SHOWDUINO_LAMP_BTN_POLARITY_NOTE   "ASSUMED_ACTIVE_LOW_UNTIL_TRACED"

static inline const char *showduino_lamp_gpio_label(int pin) {
  return pin < 0 ? "UNCONFIRMED" : "ASSIGNED";
}

#endif
