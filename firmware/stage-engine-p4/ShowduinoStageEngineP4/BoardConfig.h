#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>

/*
 * Showduino Stage Engine (ESP32-P4) — Waveshare ESP32-P4-Module-DEV-KIT
 *
 * Authoritative command path (this hardware generation):
 *   Director --ESP-NOW--> ESP32-S3 Comms Controller --UART--> this P4
 * Do not migrate Showduino commands to SDIO. Do not install ESP-Hosted.
 * Do not use the onboard ESP32-C6 for Showduino application firmware.
 *
 * UART to dedicated ESP32-S3 Comms Controller (application transport):
 *   P4 GPIO4 RX  <-  S3 GPIO17 TX
 *   P4 GPIO5 TX  ->  S3 GPIO18 RX
 *   115200 8N1, newline-framed ASCII.
 *   P4 RX/TX pins do not change. S3 GPIOs are defined on the S3 BoardConfig
 *   and are documented here only as the expected cable pairing.
 *
 * Onboard microSD is SDMMC slot 0 (not SPI): CLK/CMD/D0-D3 plus GPIO45
 * power and on-chip LDO VO4 (channel 4) for GPIO 39-48.
 * Boot continues if the card is missing.
 *
 * Onboard ESP32-C6 is UNUSED BY SHOWDUINO / RESERVED HARDWARE.
 * Do not require C6 firmware, ESP-NOW, SDIO, ESP-Hosted, or WebUI on C6.
 * Do not allocate these P4 pins to peripherals:
 *   GPIO14-19  RESERVED — onboard C6 SDIO (CLK=18 CMD=19 D0=14 D1=15 D2=16 D3=17)
 *   GPIO54     RESERVED — onboard C6 reset / CHIP_PU
 *   GPIO6      RESERVED — onboard C6 control (C6 GPIO2)
 *
 * Obsolete (do not use):
 *   P4 GPIO5 RX / GPIO6 TX  — old Stage Engine sketch to external SUE C3
 *   P4 GPIO18 RX / GPIO17 TX — stale SUE comment; those pins are C6 SDIO
 *   Onboard C6 GPIO4/5 UART jumpers — superseded by the dedicated S3
 */

#ifndef SHOWDUINO_COMMS_UART_BAUD
#define SHOWDUINO_COMMS_UART_BAUD      115200
#endif
#ifndef SHOWDUINO_COMMS_UART_RX_PIN
#define SHOWDUINO_COMMS_UART_RX_PIN    4
#endif
#ifndef SHOWDUINO_COMMS_UART_TX_PIN
#define SHOWDUINO_COMMS_UART_TX_PIN    5
#endif
#ifndef SHOWDUINO_COMMS_UART_RX_BUFFER
#define SHOWDUINO_COMMS_UART_RX_BUFFER 1024
#endif
#ifndef SHOWDUINO_COMMS_LINK_TIMEOUT_MS
#define SHOWDUINO_COMMS_LINK_TIMEOUT_MS 8000UL
#endif
#ifndef SHOWDUINO_COMMS_CMD_MAX
#define SHOWDUINO_COMMS_CMD_MAX        240
#endif

/* Expected S3 Comms Controller UART GPIOs — documentation / RUN:TEST hint only. */
#ifndef SHOWDUINO_COMMS_PEER_TX_PIN
#define SHOWDUINO_COMMS_PEER_TX_PIN    17
#endif
#ifndef SHOWDUINO_COMMS_PEER_RX_PIN
#define SHOWDUINO_COMMS_PEER_RX_PIN    18
#endif

/* Compatibility aliases for older local patches. Do not use in new code. */
#ifndef SHOWDUINO_C6_UART_BAUD
#define SHOWDUINO_C6_UART_BAUD         SHOWDUINO_COMMS_UART_BAUD
#endif
#ifndef SHOWDUINO_C6_UART_RX_PIN
#define SHOWDUINO_C6_UART_RX_PIN       SHOWDUINO_COMMS_UART_RX_PIN
#endif
#ifndef SHOWDUINO_C6_UART_TX_PIN
#define SHOWDUINO_C6_UART_TX_PIN       SHOWDUINO_COMMS_UART_TX_PIN
#endif
#ifndef SHOWDUINO_C6_UART_RX_BUFFER
#define SHOWDUINO_C6_UART_RX_BUFFER    SHOWDUINO_COMMS_UART_RX_BUFFER
#endif
#ifndef SHOWDUINO_C6_LINK_TIMEOUT_MS
#define SHOWDUINO_C6_LINK_TIMEOUT_MS   SHOWDUINO_COMMS_LINK_TIMEOUT_MS
#endif
#ifndef SHOWDUINO_C6_CMD_MAX
#define SHOWDUINO_C6_CMD_MAX           SHOWDUINO_COMMS_CMD_MAX
#endif

// Stage Controller onboard microSD (SDMMC 4-bit, slot 0 IOMUX)
// CLK=43  CMD=44  D0=39  D1=40  D2=41  D3=42  POWER=45 (active LOW)  LDO=4
#ifndef SHOWDUINO_SD_ENABLED
#define SHOWDUINO_SD_ENABLED           1
#endif

#define SHOWDUINO_SD_CLK_PIN           43
#define SHOWDUINO_SD_CMD_PIN           44
#define SHOWDUINO_SD_D0_PIN            39
#define SHOWDUINO_SD_D1_PIN            40
#define SHOWDUINO_SD_D2_PIN            41
#define SHOWDUINO_SD_D3_PIN            42
#define SHOWDUINO_SD_POWER_PIN         45
#define SHOWDUINO_SD_POWER_ON_LEVEL    LOW
#define SHOWDUINO_SD_LDO_CHANNEL       4
#define SHOWDUINO_SD_FREQ_KHZ          20000

#define PATH_WEBUI                     "/showduino/webui"
#define PATH_WEBUI_WWW                 PATH_WEBUI
#define PATH_EMERGENCY_AUDIO_DIR       "/showduino/audio"

// Emergency audio: prefer organised Showduino path, then SD-root copies.
#define PATH_EMERGENCY_WAV             "/showduino/audio/emergency.wav"
#define PATH_EMERGENCY_WAV_ROOT        "/emergency.wav"
#define PATH_EMERGENCY_MP3             "/showduino/audio/emergency.mp3"
#define PATH_EMERGENCY_MP3_ROOT        "/emergency.mp3"

/*
 * Physical E-stop: momentary push button, GPIO25 to GND.
 * INPUT_PULLUP: released = HIGH, pressed = LOW. 30 ms debounce.
 * GPIO25 is a trigger input only. The P4 latches emergency in software.
 * Release / second press does not clear. Director EMERGENCY:CLEAR does,
 * including after a physical press, once the button is released (debounced).
 * CLEAR is rejected only while the button is still held LOW.
 */
#ifndef SHOWDUINO_ESTOP_GPIO
#define SHOWDUINO_ESTOP_GPIO           25
#endif
#ifndef SHOWDUINO_ESTOP_ASSERTED_LEVEL
#define SHOWDUINO_ESTOP_ASSERTED_LEVEL LOW
#endif
#ifndef SHOWDUINO_ESTOP_PIN_MODE
#define SHOWDUINO_ESTOP_PIN_MODE       INPUT_PULLUP
#endif
#ifndef SHOWDUINO_ESTOP_DEBOUNCE_MS
#define SHOWDUINO_ESTOP_DEBOUNCE_MS    30UL
#endif

/*
 * Emergency NeoPixel DATA on GPIO24.
 * Count / colour order / timing come from the existing implementation
 * (not from a newly confirmed strip datasheet).
 */
#ifndef SHOWDUINO_EMERGENCY_PIXEL_ENABLED
#define SHOWDUINO_EMERGENCY_PIXEL_ENABLED  1
#endif

#define SHOWDUINO_EMERGENCY_PIXEL_PIN        24
#define SHOWDUINO_EMERGENCY_PIXEL_COUNT      100
#define SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS 255

// PCM5102A I2S DAC (physically wired).
#ifndef P4_AUDIO_I2S_BCLK
#define P4_AUDIO_I2S_BCLK   21
#endif
#ifndef P4_AUDIO_I2S_WS
#define P4_AUDIO_I2S_WS     20
#endif
#ifndef P4_AUDIO_I2S_DOUT
#define P4_AUDIO_I2S_DOUT   22
#endif

/*
 * Showduino Plug-in Bus (I²C) — Waveshare ESP32-P4-Module-DEV-KIT
 *
 * Official board I²C (Waveshare wiki + schematic nets ESP_I2C_SDA/SCL):
 *   SDA = GPIO7
 *   SCL = GPIO8
 * Exposed on the dedicated SH1.0 I²C header and on the 40-pin header
 * (Raspberry Pi-style pin 3 / pin 5). Shared with onboard ES8311 (0x18)
 * and MIPI CSI/DSI touch/control. Board already has 3.3V I²C pull-ups;
 * do not add 5V pull-ups. Showduino show audio remains PCM5102A I2S,
 * not the ES8311.
 *
 * Default 100 kHz. 3.3V logic only on SDA/SCL.
 */
#ifndef SHOWDUINO_PLUGIN_BUS_ENABLED
#define SHOWDUINO_PLUGIN_BUS_ENABLED   1
#endif
#ifndef SHOWDUINO_PLUGIN_BUS_ID
#define SHOWDUINO_PLUGIN_BUS_ID        0
#endif
#ifndef SHOWDUINO_PLUGIN_BUS_SDA_PIN
#define SHOWDUINO_PLUGIN_BUS_SDA_PIN   7
#endif
#ifndef SHOWDUINO_PLUGIN_BUS_SCL_PIN
#define SHOWDUINO_PLUGIN_BUS_SCL_PIN   8
#endif
#ifndef SHOWDUINO_PLUGIN_BUS_HZ
#define SHOWDUINO_PLUGIN_BUS_HZ        100000UL
#endif
#ifndef SHOWDUINO_PLUGIN_BUS_TIMEOUT_MS
#define SHOWDUINO_PLUGIN_BUS_TIMEOUT_MS 50UL
#endif

#define PATH_PLUGINS                   "/showduino/plugins"
#define PATH_PLUGIN_DEVICES            "/showduino/plugins/devices"
#define PATH_PLUGIN_REGISTRY           "/showduino/plugins/registry.json"

#define PATH_DIAGNOSTICS               "/showduino/diagnostics"
#define PATH_DIAG_LAST_TEST            PATH_DIAGNOSTICS "/last-test.txt"
#define PATH_DIAG_PROBE                "/showduino/.diagnostic_test.tmp"

#endif /* SHOWDUINO_STAGE_BOARD_CONFIG_H */
