#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>
#include "../../../protocol/showduino_log.h"

#ifndef SHOWDUINO_P4_FIRMWARE_VERSION
#define SHOWDUINO_P4_FIRMWARE_VERSION      "0.5.0"
#endif

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
#ifndef SHOWDUINO_DIRECTOR_ABSENCE_MS
#define SHOWDUINO_DIRECTOR_ABSENCE_MS  12000UL
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
#define PATH_EMERGENCY_AUDIO_DIR       "/showduino/audio/system"
#define PATH_AUDIO_SYSTEM              "/showduino/audio/system"
#ifndef PATH_AUDIO_SYSTEM_LIBRARY
#define PATH_AUDIO_SYSTEM_LIBRARY      "/showduino/audio/show_machine"
#endif

/*
 * Canonical P4 system / safety WAV set.
 * Playback also accepts the same filenames from PATH_AUDIO_SYSTEM_LIBRARY
 * when the canonical copy is missing or not a valid engine WAV.
 * shutdown.wav may exist on SD but must not be played in this generation.
 */
#define PATH_SYSTEM_BOOT_WAV           PATH_AUDIO_SYSTEM "/boot.wav"
#define PATH_SYSTEM_EMERGENCY_WAV      PATH_AUDIO_SYSTEM "/emergency.wav"
#define PATH_SYSTEM_BEEP_WAV           PATH_AUDIO_SYSTEM "/beep.wav"
#define PATH_SYSTEM_TONE_WAV           PATH_AUDIO_SYSTEM "/tone.wav"
#define PATH_SYSTEM_ERROR_WAV          PATH_AUDIO_SYSTEM "/error.wav"
#define PATH_SYSTEM_ACCEPTED_WAV       PATH_AUDIO_SYSTEM "/accepted.wav"
#define PATH_SYSTEM_COMPLETE_WAV       PATH_AUDIO_SYSTEM "/complete.wav"
#define PATH_SYSTEM_SHUTDOWN_WAV       PATH_AUDIO_SYSTEM "/shutdown.wav"

/* Compatibility alias — emergency.wav now lives in the system set. */
#define PATH_EMERGENCY_WAV             PATH_SYSTEM_EMERGENCY_WAV
#define PATH_EMERGENCY_WAV_ROOT        "/emergency.wav"
#define PATH_EMERGENCY_MP3             "/showduino/audio/emergency.mp3"
#define PATH_EMERGENCY_MP3_ROOT        "/emergency.mp3"

/*
 * Physical emergency input: momentary pushbutton, GPIO25 to GND.
 * INPUT_PULLUP: released = HIGH (healthy), pressed = LOW (emergency).
 * GPIO25 is a trigger input only. The P4 latches emergency in software.
 * Release does not clear. Director confirmation clears only after a valid
 * physical long-hold request, and only once the button is released again.
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
#ifndef SHOWDUINO_ESTOP_LOCATE_PRESS_COUNT
#define SHOWDUINO_ESTOP_LOCATE_PRESS_COUNT 8
#endif
#ifndef SHOWDUINO_ESTOP_LOCATE_WINDOW_MS
#define SHOWDUINO_ESTOP_LOCATE_WINDOW_MS   6000UL
#endif
#ifndef SHOWDUINO_ESTOP_CLEAR_HOLD_MS
#define SHOWDUINO_ESTOP_CLEAR_HOLD_MS      3000UL
#endif
#ifndef SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS
#define SHOWDUINO_ESTOP_CLEAR_REQUEST_TIMEOUT_MS 12000UL
#endif

/*
 * Dedicated emergency/signage NeoPixel line on GPIO24.
 *
 * Physical convention:
 *   - each emergency-exit sign is a fixed bundle of 10 NeoPixels;
 *   - up to 100 pixels = up to 10 signs on one chain;
 *   - NORMAL: first pixel of each 10-pixel group is GREEN, remaining 9 OFF;
 *   - EMERGENCY: every pixel in every group is full-bright WHITE;
 *   - all groups are written into one frame, then transmitted together.
 *
 * The emergency/signage line is safety-owned. Productions cannot repurpose it.
 */
#ifndef SHOWDUINO_EMERGENCY_PIXEL_ENABLED
#define SHOWDUINO_EMERGENCY_PIXEL_ENABLED  1
#endif
#define SHOWDUINO_EMERGENCY_PIXEL_PIN        24
#define SHOWDUINO_EMERGENCY_PIXEL_COUNT      100
#define SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS 255
#define SHOWDUINO_EMERGENCY_SIGN_PIXELS       10
#define SHOWDUINO_EMERGENCY_SIGN_LOCATOR_INDEX 0
#define SHOWDUINO_EMERGENCY_SIGN_NORMAL_R      0
#define SHOWDUINO_EMERGENCY_SIGN_NORMAL_G    255
#define SHOWDUINO_EMERGENCY_SIGN_NORMAL_B      0

/*
 * P4 local theatrical pixels.
 *   GPIO24 = dedicated emergency/signage line above.
 *   GPIO23 = the only general-purpose local Show Pixel Line.
 *
 * GPIO23 supports independent segments and a reusable non-blocking FX library.
 * Emergency policy sits above ALL FX: when the Showduino emergency latch is
 * active, every pixel on GPIO23 is forced full-bright WHITE. Clearing emergency
 * leaves show pixels OFF; interrupted effects never auto-resume.
 *
 * Final-install wiring standard for EACH pixel data output:
 *   P4/5V logic buffer -> 470 ohm series resistor -> first pixel DIN
 * Put the resistor near the controller/logic buffer. Common P4/pixel ground is
 * mandatory. A 74AHCT125/74HCT125-class 5V logic buffer is recommended for
 * final/long-cable installs. The 470 ohm resistor is not a level shifter.
 */
#ifndef SHOWDUINO_SHOW_PIXEL_PIN
#define SHOWDUINO_SHOW_PIXEL_PIN             23
#endif
#ifndef SHOWDUINO_SHOW_PIXEL_ENABLED
#define SHOWDUINO_SHOW_PIXEL_ENABLED         1
#endif
#ifndef SHOWDUINO_SHOW_PIXEL_COUNT
#define SHOWDUINO_SHOW_PIXEL_COUNT           100
#endif
#ifndef SHOWDUINO_SHOW_PIXEL_BRIGHTNESS
#define SHOWDUINO_SHOW_PIXEL_BRIGHTNESS      255
#endif
#ifndef SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS
#define SHOWDUINO_SHOW_PIXEL_MAX_SEGMENTS    16
#endif
#ifndef SHOWDUINO_SHOW_PIXEL_FRAME_MS
#define SHOWDUINO_SHOW_PIXEL_FRAME_MS        20UL
#endif
#ifndef SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS
#define SHOWDUINO_PIXEL_DATA_RESISTOR_OHMS   470
#endif

/*
 * LEGACY PCM5102A (retired live path).
 * Historical external I2S DAC on GPIO20/21/22. That driver wrote "playing"
 * while the onboard speaker stayed silent. Do not compile it in.
 * Attraction / programme audio is the Audio Node, not this P4.
 */
#ifndef SHOWDUINO_LEGACY_PCM5102A_AUDIO
#define SHOWDUINO_LEGACY_PCM5102A_AUDIO  0
#endif
#if SHOWDUINO_LEGACY_PCM5102A_AUDIO
#define P4_AUDIO_I2S_BCLK   21
#define P4_AUDIO_I2S_WS     20
#define P4_AUDIO_I2S_DOUT   22
#endif

// -----------------------------------------------------------------------------
// Onboard ES8311 + NS4150B — SHOWDUINO SYSTEM / SAFETY AUDIO (LIVE)
//
// Verified against:
//   Waveshare ESP32-P4-Module-DEV-KIT wiki I2S table
//   ESP-IDF P4 ES8311 examples (I2S_DO=GPIO9, I2S_DI=GPIO11)
//   Arduino-ESP32 3.3.11 waveshare_p4_poe_eth pins_arduino.h
//
// NOTE: Arduino 3.3.11 waveshare_p4_poe_eth names I2S_DOUT=11 / I2S_DIN=9.
// Those labels are swapped versus the Waveshare wiki and working ESP-IDF
// P4 examples. Showduino follows the wiki / ESP-IDF mapping:
//   MCU I2S DOUT = GPIO9  = ES8311 DSDIN
//   MCU I2S DIN  = GPIO11 = ES8311 ASDOUT (unused for playback)
//
// GPIO7/8 are shared with the Plug-in Bus. ES8311 address is 0x18.
// -----------------------------------------------------------------------------
#ifndef P4_SYSTEM_AUDIO_I2C_SDA
#define P4_SYSTEM_AUDIO_I2C_SDA       7
#endif
#ifndef P4_SYSTEM_AUDIO_I2C_SCL
#define P4_SYSTEM_AUDIO_I2C_SCL       8
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_DOUT
#define P4_SYSTEM_AUDIO_I2S_DOUT      9
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_WS
#define P4_SYSTEM_AUDIO_I2S_WS        10
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_DIN
#define P4_SYSTEM_AUDIO_I2S_DIN       11
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_BCLK
#define P4_SYSTEM_AUDIO_I2S_BCLK      12
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_MCLK
#define P4_SYSTEM_AUDIO_I2S_MCLK      13
#endif
#ifndef P4_SYSTEM_AUDIO_PA_ENABLE
#define P4_SYSTEM_AUDIO_PA_ENABLE     53
#endif
#ifndef P4_SYSTEM_AUDIO_PA_ON_LEVEL
#define P4_SYSTEM_AUDIO_PA_ON_LEVEL   HIGH
#endif
#ifndef P4_ES8311_I2C_ADDR
#define P4_ES8311_I2C_ADDR            0x18U
#endif

/* Live P4 audio aliases — onboard ES8311, not the retired PCM5102A. */
#ifndef P4_AUDIO_I2S_BCLK
#define P4_AUDIO_I2S_BCLK   P4_SYSTEM_AUDIO_I2S_BCLK
#endif
#ifndef P4_AUDIO_I2S_WS
#define P4_AUDIO_I2S_WS     P4_SYSTEM_AUDIO_I2S_WS
#endif
#ifndef P4_AUDIO_I2S_DOUT
#define P4_AUDIO_I2S_DOUT   P4_SYSTEM_AUDIO_I2S_DOUT
#endif

/*
 * ESP32-P4 exposes one I2S peripheral. It is owned by onboard system audio.
 * The retired PCM5102A path must not run at the same time.
 */

/*
 * Showduino Plug-in Bus (I²C) — Waveshare ESP32-P4-Module-DEV-KIT
 *
 * Official board I²C (Waveshare wiki + schematic nets ESP_I2C_SDA/SCL):
 *   SDA = GPIO7
 *   SCL = GPIO8
 * Exposed on the dedicated SH1.0 I²C header and on the 40-pin header
 * (Raspberry Pi-style pin 3 / pin 5). Shared with onboard ES8311 (0x18)
 * and MIPI CSI/DSI touch/control. Board already has 3.3V I²C pull-ups;
 * do not add 5V pull-ups. The onboard ES8311 is Showduino system/safety
 * audio. Attraction/programme audio is the Audio Node.
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
#define PATH_CONFIG                    "/showduino/config"
#define PATH_PLUGIN_BUS_CONFIG         "/showduino/config/plugin-bus.json"
#define PATH_NETWORK_CONFIG            "/showduino/config/network.json"
#define PATH_SYSTEM_CONFIG             "/showduino/config/system.json"
#define PATH_E131_CONFIG               "/showduino/config/e131.json"
#define PATH_PIXELS_CONFIG             "/showduino/config/pixels.json"
#define PATH_NODES_CONFIG              "/showduino/config/nodes.json"
#define PATH_DIRECTOR_CONFIG           "/showduino/config/director.json"
#define PATH_PRODUCTIONS               "/showduino/productions"
#define PATH_ASSETS                    "/showduino/assets"
#define PATH_LOGS                      "/showduino/logs"
#define PATH_BACKUPS                   "/showduino/backups"
#define PATH_IMPORT                    "/showduino/import"
#define PATH_EXPORT                    "/showduino/export"
#define PATH_RECOVERY                  "/showduino/recovery"
#define PATH_SYSTEM_META               "/showduino/system"
#define PATH_STORAGE_VERSION           "/showduino/system/storage-version.json"
#define PATH_LAST_BOOT                 "/showduino/system/last-boot.json"

#define PATH_DIAGNOSTICS               "/showduino/diagnostics"
#define PATH_DIAG_LAST_TEST            PATH_DIAGNOSTICS "/last-test.txt"
#define PATH_DIAG_PROBE                "/showduino/.diagnostic_test.tmp"

/*
 * Internal ESP32-P4 RTC. No DS3231 on this generation.
 * GPIO0/1 are the board 32.768 kHz crystal path — do not reassign.
 */
#ifndef SHOWDUINO_RTC_ENABLED
#define SHOWDUINO_RTC_ENABLED          1
#endif
#ifndef SHOWDUINO_RTC_SYNC_FLOOR
#define SHOWDUINO_RTC_SYNC_FLOOR       1600000000UL
#endif
#ifndef SHOWDUINO_RTC_PUBLISH_MS
#define SHOWDUINO_RTC_PUBLISH_MS       1000UL
#endif

/*
 * Onboard Ethernet — Waveshare ESP32-P4-Module-DEV-KIT
 * PHY: IP101GRI over RMII. Arduino-ESP32 3.3.11 alias ETH_PHY_TLK110 / ETH_PHY_IP101.
 * Confirmed from Waveshare wiki + Arduino variant pins_arduino.h
 * (esp32p4 and waveshare_p4_poe_eth). Generic FQBN esp32p4 already defines these.
 *
 *   TX_EN    GPIO49
 *   TXD0     GPIO34
 *   TXD1     GPIO35
 *   RXD0     GPIO29
 *   RXD1     GPIO30
 *   CRS_DV   GPIO28
 *   REF_CLK  GPIO50   50 MHz from PHY, EMAC_CLK_EXT_IN
 *   MDC      GPIO31
 *   MDIO     GPIO52
 *   RESET    GPIO51
 *   PHY addr 1
 *
 * These nets do not collide with current Showduino UART/I2C/I2S/SD/E-stop pins.
 * Do not reassign them. Internet is not required.
 */
/*
 * No discrete user LED is assigned on this Waveshare board.
 * GPIO10 is onboard ES8311 I2S LRCK/WS (Waveshare wiki + schematic I2S_LRCK).
 * The old sketch STATUS_LED_PIN=10 was a stale placeholder and must not drive
 * that codec net. Do not invent a replacement LED GPIO here.
 */
#ifndef SHOWDUINO_STATUS_LED_PIN
#define SHOWDUINO_STATUS_LED_PIN       -1
#endif

#ifndef SHOWDUINO_ETH_ENABLED
#define SHOWDUINO_ETH_ENABLED          1
#endif
#define SHOWDUINO_ETH_PHY_ADDR         1
#define SHOWDUINO_ETH_MDC_PIN          31
#define SHOWDUINO_ETH_MDIO_PIN         52
#define SHOWDUINO_ETH_POWER_PIN        51
#define SHOWDUINO_ETH_TX_EN_PIN        49
#define SHOWDUINO_ETH_TXD0_PIN         34
#define SHOWDUINO_ETH_TXD1_PIN         35
#define SHOWDUINO_ETH_RXD0_PIN         29
#define SHOWDUINO_ETH_RXD1_PIN         30
#define SHOWDUINO_ETH_CRS_DV_PIN       28
#define SHOWDUINO_ETH_REFCLK_PIN       50

#endif /* SHOWDUINO_STAGE_BOARD_CONFIG_H */