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
 * Production GPIOs were physically wired 2026-09-12:
 *   GPIO4  mic / blow ADC      ADC1_CH3
 *   GPIO5  ambient light ADC   ADC1_CH4
 *   GPIO6  voltage ADC         ADC1_CH5
 *   GPIO7  ignition button     GPIO7 -> switch -> GND
 *   GPIO8  NeoPixel Jewel DATA 7 pixels
 *   GPIO17 S3 TX -> Fermion RX
 *   GPIO18 S3 RX <- Fermion TX
 *
 * SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS remains for spare-board development
 * only and must never be combined with the confirmed production map.
 */

#define SHOWDUINO_LAMP_NODE_FW             "0.3.3"
#define SHOWDUINO_LAMP_NODE_BOARD          "ESP32-S3 Dev Module (Lamp Node)"
#define SHOWDUINO_LAMP_NODE_TARGET         "S3"

#ifndef SHOWDUINO_LAMP_PINS_CONFIRMED
#define SHOWDUINO_LAMP_PINS_CONFIRMED      1
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
 * Fermion DFPlayer Pro DFR0768 — local lamp FX only. UART 115200, no BUSY pin.
 * Powered from the lamp 5V rail with common ground. S3 UART is 3.3V.
 * Playback is fire-and-forget AT commands. flameloop.mp3 uses PLAYMODE=2.
 * The main loop never waits for a track to finish.
 */

/*
 * ADC1 only for sensors (ESP32-S3 GPIO1–GPIO10). GPIO4/5/6 are ADC1_CH3/4/5.
 * Wi-Fi / ESP-NOW conflicts with ADC2. Voltage millivolts stay UNCALIBRATED
 * until a divider scale is stored. Do not invent 5.00 V.
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
#elif SHOWDUINO_LAMP_PINS_CONFIRMED
#define SHOWDUINO_LAMP_PIXEL_PIN           8
#define SHOWDUINO_LAMP_BTN_IGNITE          7
#define SHOWDUINO_LAMP_MIC_PIN             4
#define SHOWDUINO_LAMP_LIGHT_PIN           5
#define SHOWDUINO_LAMP_VOLT_PIN            6
#define SHOWDUINO_LAMP_FERMION_TX_PIN      17
#define SHOWDUINO_LAMP_FERMION_RX_PIN      18
#define SHOWDUINO_LAMP_PIN_SOURCE          "PHYSICAL_CONFIRMED"
#else
#define SHOWDUINO_LAMP_PIXEL_PIN           (-1)
#define SHOWDUINO_LAMP_BTN_IGNITE          (-1)
#define SHOWDUINO_LAMP_MIC_PIN             (-1)
#define SHOWDUINO_LAMP_LIGHT_PIN           (-1)
#define SHOWDUINO_LAMP_VOLT_PIN            (-1)
#define SHOWDUINO_LAMP_FERMION_TX_PIN      (-1)
#define SHOWDUINO_LAMP_FERMION_RX_PIN      (-1)
#define SHOWDUINO_LAMP_PIN_SOURCE          "UNCONFIRMED"
#endif

#if SHOWDUINO_LAMP_PINS_CONFIRMED && !SHOWDUINO_LAMP_DEV_PLACEHOLDER_PINS
#if SHOWDUINO_LAMP_MIC_PIN != 4 || SHOWDUINO_LAMP_LIGHT_PIN != 5 || \
    SHOWDUINO_LAMP_VOLT_PIN != 6 || SHOWDUINO_LAMP_BTN_IGNITE != 7 || \
    SHOWDUINO_LAMP_PIXEL_PIN != 8 || SHOWDUINO_LAMP_FERMION_TX_PIN != 17 || \
    SHOWDUINO_LAMP_FERMION_RX_PIN != 18
#error "Confirmed Lamp Node pin map does not match the physical wiring"
#endif
#if SHOWDUINO_LAMP_MIC_PIN < 1 || SHOWDUINO_LAMP_MIC_PIN > 10 || \
    SHOWDUINO_LAMP_LIGHT_PIN < 1 || SHOWDUINO_LAMP_LIGHT_PIN > 10 || \
    SHOWDUINO_LAMP_VOLT_PIN < 1 || SHOWDUINO_LAMP_VOLT_PIN > 10
#error "Lamp analog sensors must stay on ADC1 (GPIO1-10)"
#endif
#endif

/* Physical striker: GPIO7 -> momentary switch -> GND. INPUT_PULLUP, pressed=LOW. */
#define SHOWDUINO_LAMP_BTN_ACTIVE_LOW      1
#define SHOWDUINO_LAMP_BTN_POLARITY_NOTE   "GPIO7_TO_GND_ACTIVE_LOW"

static inline const char *showduino_lamp_gpio_label(int pin) {
  return pin < 0 ? "UNCONFIRMED" : "ASSIGNED";
}

#endif
