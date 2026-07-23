#ifndef SHOWDUINO_C3_BOARD_CONFIG_H
#define SHOWDUINO_C3_BOARD_CONFIG_H

#include <Arduino.h>
#include "../../../protocol/showduino_protocol_version.h"

#define SHOWDUINO_ESPNOW_CHANNEL 1

// Stage 4+ — Wi-Fi front door for Showduino Studio (static assets on C3, API on P4)
#define SHOWDUINO_WEBUI_ENABLED 1
#define SHOWDUINO_WEBUI_AP_SSID "Showduino-Studio"
#define SHOWDUINO_WEBUI_AP_PASSWORD "showduino"
#define SHOWDUINO_WEBUI_MDNS "showduino-studio"

#define P4_UART_BAUD   115200
#define P4_UART_RX_PIN 20
#define P4_UART_TX_PIN 21

// Stage 5 — Device Manager heartbeat thresholds (ms since last sighting)
#define SHOWDUINO_DEVICE_HB_ONLINE_MS   5000UL
#define SHOWDUINO_DEVICE_HB_WARNING_MS 12000UL
#define SHOWDUINO_DEVICE_HB_OFFLINE_MS 20000UL
#define SHOWDUINO_WEBSOCKET_PORT 81

#define SHOWDUINO_C3_FW_VERSION "0.5.0-c3"
#define SHOWDUINO_NODE_SUE_NAME "SUE"
#define SHOWDUINO_NODE_IAN_NAME "IAN"

/*
 * Stage 7.5 — DS3231 RTC on SUE (ESP32-C3 SuperMini)
 *
 * Existing hardware on this board:
 *   GPIO20 / GPIO21 = UART to IAN (P4) — reserved
 *   GPIO18 / GPIO19 = native USB — reserved
 *   GPIO8 / GPIO9   = strapping (+ LED / BOOT) — avoided
 *   GPIO2           = strapping — avoided
 *   GPIO4 / GPIO5   = I2C SDA / SCL (shared bus)
 *
 * SQW/INT (often labelled DS on modules):
 *   Open-drain alarm interrupt for timed shows.
 *   Chosen GPIO6 — free, non-strapping, unused by UART/USB/I2C.
 */
#define RTC_ENABLED            1
#define RTC_TYPE               "DS3231"
#define RTC_SDA_PIN            4
#define RTC_SCL_PIN            5
#define RTC_SQW_PIN            6
#define RTC_SQW_ENABLED        1
#define RTC_I2C_PORT           0
#define RTC_I2C_FREQUENCY      100000
#define RTC_TIMEZONE           "UTC"
#define RTC_DST_ENABLED        0
#define RTC_UPDATE_INTERVAL_MS  1000UL

#endif