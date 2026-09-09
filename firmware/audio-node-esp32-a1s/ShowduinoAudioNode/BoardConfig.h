#ifndef SHOWDUINO_AUDIO_NODE_BOARD_CONFIG_H
#define SHOWDUINO_AUDIO_NODE_BOARD_CONFIG_H

#include "../../../protocol/showduino_log.h"

/*
 * Ai-Thinker ESP32-Audio-Kit V2.2 A161 — ESP32-A1S + ES8388
 *
 * Verified against the official V2.2 specification and NuttX / ESP-ADF
 * Audio-Kit maps. This is NOT the AC101 A1S (often silkscreen 2379).
 *
 * Factory observation for the development unit (identity metadata only):
 *   ESP32 rev 3, 4 MB flash, DOUT 40 MHz, Ai-Thinker firmware v1.1.0
 *   Wi-Fi MAC discovered at runtime — never hard-coded.
 *
 * There is no KEY7. RST is hardware-only. BOOT is GPIO0 / I2S MCLK.
 *
 * Arduino FQBN (match factory flash; do not assume PSRAM).
 * Arduino-ESP32 3.3.11 has no dout option — compile with dio:
 *   esp32:esp32:esp32:PSRAM=disabled,FlashSize=4M,PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40
 */

#define SHOWDUINO_AUDIO_NODE_FW            "0.4.1"
#define SHOWDUINO_AUDIO_NODE_BOARD         "ESP32-A1S Audio Kit V2.2 A161"
#define SHOWDUINO_AUDIO_NODE_CODEC         "ES8388"

#define SHOWDUINO_ESPNOW_CHANNEL           1
#define SHOWDUINO_AUDIO_ANNOUNCE_MS        3000UL
#define SHOWDUINO_AUDIO_ANNOUNCE_SEARCH_MS 2500UL
#define SHOWDUINO_AUDIO_VOLUME_DEBOUNCE_MS 1500UL
#define SHOWDUINO_AUDIO_KEY_LONG_MS        700UL

#define SHOWDUINO_AUDIO_I2C_ADDR           0x10
#define SHOWDUINO_AUDIO_I2C_ADDR_ALT       0x11

#ifndef SHOWDUINO_AUDIO_I2C_SDA
#define SHOWDUINO_AUDIO_I2C_SDA            33
#endif
#ifndef SHOWDUINO_AUDIO_I2C_SCL
#define SHOWDUINO_AUDIO_I2C_SCL            32
#endif

#ifndef SHOWDUINO_AUDIO_I2S_MCLK
#define SHOWDUINO_AUDIO_I2S_MCLK           0
#endif
#ifndef SHOWDUINO_AUDIO_I2S_BCLK
#define SHOWDUINO_AUDIO_I2S_BCLK           27
#endif
#ifndef SHOWDUINO_AUDIO_I2S_WS
#define SHOWDUINO_AUDIO_I2S_WS             25
#endif
#ifndef SHOWDUINO_AUDIO_I2S_DOUT
#define SHOWDUINO_AUDIO_I2S_DOUT           26
#endif
#ifndef SHOWDUINO_AUDIO_I2S_DIN
#define SHOWDUINO_AUDIO_I2S_DIN            35
#endif

#ifndef SHOWDUINO_AUDIO_PA_PIN
#define SHOWDUINO_AUDIO_PA_PIN             21
#endif
#ifndef SHOWDUINO_AUDIO_PA_ON_LEVEL
#define SHOWDUINO_AUDIO_PA_ON_LEVEL        HIGH
#endif

#ifndef SHOWDUINO_AUDIO_SD_SCK
#define SHOWDUINO_AUDIO_SD_SCK             14
#endif
#ifndef SHOWDUINO_AUDIO_SD_MISO
#define SHOWDUINO_AUDIO_SD_MISO            2
#endif
#ifndef SHOWDUINO_AUDIO_SD_MOSI
#define SHOWDUINO_AUDIO_SD_MOSI            15
#endif
#ifndef SHOWDUINO_AUDIO_SD_CS
#define SHOWDUINO_AUDIO_SD_CS              13
#endif

/* Official V2.2 keys. Commissioning map, not factory silk labels. */
#ifndef SHOWDUINO_AUDIO_KEY1
#define SHOWDUINO_AUDIO_KEY1               36  /* PLAY/STOP local test */
#endif
#ifndef SHOWDUINO_AUDIO_KEY2
#define SHOWDUINO_AUDIO_KEY2               -1  /* GPIO13 = SD CS — disabled */
#endif
#ifndef SHOWDUINO_AUDIO_KEY3
#define SHOWDUINO_AUDIO_KEY3               19  /* volume down */
#endif
#ifndef SHOWDUINO_AUDIO_KEY4
#define SHOWDUINO_AUDIO_KEY4               23  /* volume up — VSPI MOSI unused */
#endif
#ifndef SHOWDUINO_AUDIO_KEY5
#define SHOWDUINO_AUDIO_KEY5               18  /* previous test asset */
#endif
#ifndef SHOWDUINO_AUDIO_KEY6
#define SHOWDUINO_AUDIO_KEY6               5   /* next test asset */
#endif

/*
 * External one-pixel WS2812 / NeoPixel status indicator.
 *
 * The development Audio Node has no useful populated onboard status LED.
 * GPIO22 was already reserved by Showduino for the old LED4 assumption and
 * is not used by the codec, SD, keys, PA, headphone detect, or microphone.
 * It is therefore the dedicated external status-pixel data output.
 *
 * This is diagnostics/connectivity only — never a programme/show pixel.
 */
#ifndef SHOWDUINO_AUDIO_STATUS_PIXEL_PIN
#define SHOWDUINO_AUDIO_STATUS_PIXEL_PIN   22
#endif
#ifndef SHOWDUINO_AUDIO_STATUS_PIXEL_COUNT
#define SHOWDUINO_AUDIO_STATUS_PIXEL_COUNT 1
#endif
#ifndef SHOWDUINO_AUDIO_STATUS_PIXEL_ORDER
#define SHOWDUINO_AUDIO_STATUS_PIXEL_ORDER (NEO_GRB + NEO_KHZ800)
#endif

#ifndef SHOWDUINO_AUDIO_HP_DETECT_PIN
#define SHOWDUINO_AUDIO_HP_DETECT_PIN      39  /* LOW = jack inserted */
#endif
#ifndef SHOWDUINO_AUDIO_SD_DETECT_PIN
#define SHOWDUINO_AUDIO_SD_DETECT_PIN      34  /* LOW = card present */
#endif

#define PATH_SHOWDUINO                     "/showduino"
#define PATH_AUDIO_ROOT                    "/showduino/audio"
#define PATH_AUDIO_CONFIG                  "/showduino/config/audio-node.json"
#define PATH_AUDIO_DIAG                    "/showduino/diagnostics"
#define PATH_AUDIO_TEST                    "/showduino/audio/system-test.wav"

#define SHOWDUINO_AUDIO_DEFAULT_VOLUME     80
#define SHOWDUINO_AUDIO_DEFAULT_OUTPUT     "SPEAKER"
#define SHOWDUINO_AUDIO_AP_PASSWORD        "showduino"

#endif
