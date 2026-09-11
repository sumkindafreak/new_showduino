#ifndef SHOWDUINO_S3_COMMS_BOARD_CONFIG_H
#define SHOWDUINO_S3_COMMS_BOARD_CONFIG_H

#include <Arduino.h>
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../../protocol/showduino_desk_packet.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_validation.h"
#include "../../../protocol/showduino_web_tunnel.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_radio.h"
#include "../../../protocol/showduino_gateway_wire.h"

/*
 * Showduino ESP32-S3 Comms Controller
 *
 * Target: standard ESP32-S3 Dev Module (Arduino: ESP32S3 Dev Module).
 * Do not assume SuperMini, XIAO, Waveshare S3, or other vendor boards.
 *
 * Current product path:
 *   Director ESP32-S3 --ESP-NOW--> this board --UART--> ESP32-P4 Stage Engine
 *
 * This board is a communications processor only. It must not become the
 * Stage Engine. It must not host show state, audio, pixels, SD, or plugins.
 * It may serve the browser WebUI from PROGMEM and proxy /api GETs to the P4.
 *
 * FUTURE / RESERVED / NOT IMPLEMENTED on this firmware:
 *   - Bluetooth LE
 *   - Bluetooth Classic
 *   - OTA installation / rollback
 *   - Ethernet / E1.31
 * GitHub update discovery is implemented. Firmware install is not.
 * Do not initialise Bluetooth libraries here.
 */

#define SHOWDUINO_COMMS_FIRMWARE_VERSION "0.4.0"

#ifndef SHOWDUINO_WEBUI_ENABLED
#define SHOWDUINO_WEBUI_ENABLED 1
#endif
#define SHOWDUINO_WEBUI_AP_SSID "Showduino"
/* Documented bench WPA2 secret. Change before a public venue. Not an open AP. */
#define SHOWDUINO_WEBUI_AP_PASSWORD "showduino"
#define SHOWDUINO_WEBUI_MDNS "showduino"

#define USB_DEBUG_BAUD 115200

#define SHOWDUINO_COMMS_UART_BAUD      115200
#define SHOWDUINO_COMMS_UART_CONFIG    SERIAL_8N1
#define SHOWDUINO_COMMS_UART_RX_BUFFER 32768
#define SHOWDUINO_COMMS_LINE_MAX       180
#define SHOWDUINO_COMMS_LINK_TIMEOUT_MS 8000UL
#define SHOWDUINO_COMMS_PING_TIMEOUT_MS 2000UL

/*
 * Dedicated P4 UART on UART1 — not UART0, not native USB.
 *
 * Selected pins (ESP32-S3 DevKitC-1 / WROOM-1 header convention):
 *   RX = GPIO18   (this board receives P4 TX)
 *   TX = GPIO17   (this board transmits to P4 RX)
 *
 * Why these GPIOs are safe on a standard ESP32-S3 Dev Module:
 *   - Espressif UART1 example default is TX=17 / RX=18.
 *   - Broken out on DevKitC-1 style headers.
 *   - Not GPIO19/20 (USB D-/D+). Native USB CDC stays available.
 *   - Not GPIO43/44 (U0TXD/U0RXD). UART0 debug/programming is untouched.
 *   - Not strapping pins GPIO0, GPIO3, GPIO45, GPIO46.
 *   - Not typical WROOM-1 flash/PSRAM balls GPIO26–37.
 *   - Not JTAG MTDI/MTCK/MTMS/MTDO (GPIO39–42) as a required debug path.
 *
 * Do not assume these numbers match the P4. They do not.
 * P4 remains RX=GPIO4 TX=GPIO5.
 *
 * Physical wiring:
 *   S3 GPIO17 TX  ->  P4 GPIO4 RX
 *   S3 GPIO18 RX  <-  P4 GPIO5 TX
 *   S3 GND        --  P4 GND
 */
#ifndef SHOWDUINO_COMMS_UART_RX_PIN
#define SHOWDUINO_COMMS_UART_RX_PIN    18
#endif
#ifndef SHOWDUINO_COMMS_UART_TX_PIN
#define SHOWDUINO_COMMS_UART_TX_PIN    17
#endif

#define SHOWDUINO_ESPNOW_CHANNEL       1

/*
 * Onboard addressable RGB — COMMS STATUS only.
 *
 * Current Showduino Comms S3 is ESP32-S3-DevKitC-1 v1.0 style hardware:
 *   RGB on GPIO48 (WS2812).
 * Espressif DevKitC-1 v1.1 moved the same LED to GPIO38.
 * Keep the pin here so the status service does not hard-code 48.
 *
 * This LED is not a show effect, not production-owned, and not the
 * future Show Pixel Engine.
 */
#ifndef SHOWDUINO_COMMS_RGB_PIN
#define SHOWDUINO_COMMS_RGB_PIN 48
#endif
#ifndef SHOWDUINO_COMMS_RGB_COUNT
#define SHOWDUINO_COMMS_RGB_COUNT 1
#endif
#ifndef SHOWDUINO_COMMS_RGB_BRIGHTNESS
#define SHOWDUINO_COMMS_RGB_BRIGHTNESS 200                                                                   
#endif
#ifndef SHOWDUINO_COMMS_RGB_GRACE_MS
#define SHOWDUINO_COMMS_RGB_GRACE_MS 2500UL
#endif
#ifndef SHOWDUINO_COMMS_RGB_SYNC_MS
#define SHOWDUINO_COMMS_RGB_SYNC_MS 1500UL
#endif

#endif /* SHOWDUINO_S3_COMMS_BOARD_CONFIG_H */
