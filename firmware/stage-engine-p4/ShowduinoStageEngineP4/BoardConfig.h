#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>

/*
 * Showduino Stage Engine (ESP32-P4) - board / feature config.
 *
 * Onboard microSD is SDMMC slot 0 (not SPI): CLK/CMD/D0-D3 plus GPIO45
 * power and on-chip LDO VO4 (channel 4) for GPIO 39-48.
 * Boot continues if the card is missing.
 *
 * UART to Communications Engine (SUE C3) is the sketch mapping:
 *   P4 GPIO18 RX <- SUE TX
 *   P4 GPIO17 TX -> SUE RX
 */

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

#endif /* SHOWDUINO_STAGE_BOARD_CONFIG_H */
