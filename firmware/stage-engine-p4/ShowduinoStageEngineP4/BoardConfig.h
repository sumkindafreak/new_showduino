#ifndef SHOWDUINO_STAGE_BOARD_CONFIG_H
#define SHOWDUINO_STAGE_BOARD_CONFIG_H

#include <Arduino.h>

/*
 * Showduino Show Engine / Stage Controller (ESP32-P4)
 * Waveshare ESP32-P4-Module-DEV-KIT
 *
 * Hardware baseline: 2026-08-25
 * - ESP32-P4 = Show Engine
 * - onboard ESP32-C6 = Communications Engine hardware target
 * - P4 RTC / rechargeable RTC battery connection replaces external DS3231
 * - onboard ES8311 + NS4150B = Showduino/system sounds
 * - external PCM5102A = dedicated show/programme audio
 * - emergency NeoPixel = GPIO24
 * - momentary emergency button = GPIO25
 *
 * The old external C3 UART mapping remains in current firmware only as a
 * compatibility/rollback path while the onboard-C6 transport is qualified.
 * Do not treat it as the final product topology.
 */

// -----------------------------------------------------------------------------
// Stage Controller onboard microSD
// SDMMC 4-bit, slot 0 IOMUX. This is NOT an SPI SD breakout.
// CLK=43 CMD=44 D0=39 D1=40 D2=41 D3=42 POWER=45 active LOW.
// GPIO39-48 use on-chip LDO VO4 on this board path.
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Physical emergency button
// Momentary push button from GPIO25 to GND.
// INPUT_PULLUP: released = HIGH, pressed = LOW. 30 ms debounce.
//
// GPIO25 is a trigger only. The P4 latches emergency in software.
// Release / second press does not clear. Director EMERGENCY:CLEAR may clear
// only after the physical button is released and debounced.
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Emergency NeoPixel line
// -----------------------------------------------------------------------------
#ifndef SHOWDUINO_EMERGENCY_PIXEL_ENABLED
#define SHOWDUINO_EMERGENCY_PIXEL_ENABLED  1
#endif

#define SHOWDUINO_EMERGENCY_PIXEL_PIN        24
#define SHOWDUINO_EMERGENCY_PIXEL_COUNT      100
#define SHOWDUINO_EMERGENCY_PIXEL_BRIGHTNESS 255

// -----------------------------------------------------------------------------
// External PCM5102A — SHOW / PROGRAMME AUDIO
// Physically wired Stage Controller show-audio path.
// -----------------------------------------------------------------------------
#ifndef P4_AUDIO_I2S_BCLK
#define P4_AUDIO_I2S_BCLK   21
#endif
#ifndef P4_AUDIO_I2S_WS
#define P4_AUDIO_I2S_WS     20
#endif
#ifndef P4_AUDIO_I2S_DOUT
#define P4_AUDIO_I2S_DOUT   22
#endif

// Semantic aliases for new code. Keep the older P4_AUDIO_* names above for
// compatibility with existing source while the audio engine is refactored.
#ifndef P4_SHOW_AUDIO_I2S_BCLK
#define P4_SHOW_AUDIO_I2S_BCLK P4_AUDIO_I2S_BCLK
#endif
#ifndef P4_SHOW_AUDIO_I2S_WS
#define P4_SHOW_AUDIO_I2S_WS   P4_AUDIO_I2S_WS
#endif
#ifndef P4_SHOW_AUDIO_I2S_DOUT
#define P4_SHOW_AUDIO_I2S_DOUT P4_AUDIO_I2S_DOUT
#endif

// -----------------------------------------------------------------------------
// Onboard ES8311 + NS4150B — SHOWDUINO / SYSTEM AUDIO
// Waveshare ESP32-P4-Module-DEV-KIT board mapping.
//
// This configuration reserves the pins for the system-audio implementation.
// Defining them here does not by itself enable the codec driver.
// -----------------------------------------------------------------------------
#ifndef P4_SYSTEM_AUDIO_I2C_SDA
#define P4_SYSTEM_AUDIO_I2C_SDA       7
#endif
#ifndef P4_SYSTEM_AUDIO_I2C_SCL
#define P4_SYSTEM_AUDIO_I2C_SCL       8
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_DOUT
// P4 -> ES8311 DSDIN
#define P4_SYSTEM_AUDIO_I2S_DOUT      9
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_WS
#define P4_SYSTEM_AUDIO_I2S_WS        10
#endif
#ifndef P4_SYSTEM_AUDIO_I2S_DIN
// ES8311 ASDOUT -> P4, used by microphone/codec input when required.
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

/*
 * IMPORTANT: ESP32-P4 exposes one I2S peripheral. The onboard ES8311 system
 * audio and external PCM5102A show audio must be treated as an arbitrated
 * resource. Do not start two independent streams without an implementation
 * that explicitly proves that behaviour on this board.
 */

// -----------------------------------------------------------------------------
// RTC baseline
// The final Stage Controller uses the ESP32-P4 RTC domain and the Waveshare
// rechargeable RTC battery connection. No DS3231 GPIO/I2C assignment belongs
// in the final P4 board map. RTC backup behaviour still requires qualification.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Previous external C3/SUE UART — COMPATIBILITY ONLY
// Current sketches historically used:
//   P4 GPIO18 RX <- external C3 TX
//   P4 GPIO17 TX -> external C3 RX
//   115200 baud
//
// These pins must remain reserved until the onboard C6 path completely removes
// that compatibility transport from the P4 firmware.
// -----------------------------------------------------------------------------

#endif /* SHOWDUINO_STAGE_BOARD_CONFIG_H */
