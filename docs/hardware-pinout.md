# Showduino v1 Hardware Pinout Draft

This is the first central pinout document for Showduino v1.

Nothing should be considered final until it has been tested on real hardware and marked as confirmed.

## Hardware Families

Showduino v1 is designed around these main hardware blocks:

1. CYD touchscreen controller
2. Arduino Mega executor
3. SUE ESP32-S3 add-on node
4. R3 terminal / add-on nodes
5. Future custom PCB

## 1. CYD Touchscreen Controller

Source reference:

```text
sumkindafreak/Showduino-controller
```

Target:

```text
ESP32-2432S028R / CYD-style 2.8 inch touchscreen
```

### Touch Pins

```cpp
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33
```

Status:

```text
Known from previous controller code. Needs final real-hardware confirmation in v1.
```

### SD Card Pins

```cpp
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS   5
```

Status:

```text
Known from previous controller code. Needs final real-hardware confirmation in v1.
```

### CYD LEDs / Backlight

```cpp
#define CYD_LED_BLUE  17
#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define BACKLIGHT_PIN 21
```

Status:

```text
Known from previous controller code. Needs final real-hardware confirmation in v1.
```

### Serial to Mega

Previous code used:

```cpp
#define MEGA_RX 3
#define MEGA_TX 1
```

Important warning:

GPIO 1 and GPIO 3 are often USB serial pins on ESP32 boards. This may work, but it can interfere with upload/debug serial depending on board wiring.

Recommended v1 decision:

- Test existing pins first.
- If upload/debug conflicts happen, move Mega UART to a safer Serial2 pair.
- Document exact working wiring after test.

Status:

```text
Needs confirmation.
```

## 2. Arduino Mega Executor

Source references:

```text
sumkindafreak/arduino_show_controller
sumkindafreak/showduino-complete-code
```

Target:

```text
Arduino Mega 2560
```

### Relay Outputs

Recommended v1 relay map from previous stable notes:

```cpp
#define RELAY_1 22
#define RELAY_2 24
#define RELAY_3 26
#define RELAY_4 28
#define RELAY_5 30
#define RELAY_6 32
#define RELAY_7 34
#define RELAY_8 36
```

Status:

```text
Recommended as v1 baseline.
```

### Digital Inputs

Recommended v1 input map:

```cpp
#define INPUT_1 38
#define INPUT_2 40
#define INPUT_3 42
#define INPUT_4 44
#define INPUT_5 46
#define INPUT_6 48
#define INPUT_7 50
#define INPUT_8 52
```

Status:

```text
Recommended as v1 baseline.
```

### DMX Output

Recommended v1 DMX pin:

```cpp
#define DMX_PIN 3
```

Status:

```text
Recommended as v1 baseline if using DmxSimple.
```

### NeoPixel Outputs

Recommended v1 pixel outputs:

```cpp
#define PIXEL_LINE_1 4
#define PIXEL_LINE_2 5
#define PIXEL_LINE_3 15
#define PIXEL_LINE_4 16
```

Status:

```text
Recommended as v1 baseline.
```

### MP3 Players

Previous dual YX5300 / DFPlayer-style serial arrangement:

```cpp
// Player A
#define MP3_A_RX 10
#define MP3_A_TX 11

// Player B
#define MP3_B_RX 8
#define MP3_B_TX 9
```

Status:

```text
Recommended as v1 baseline, pending chosen audio module.
```

### Status LEDs

Recommended from previous executor design:

```cpp
#define STATUS_LED 13
#define COMM_LED   12
#define MP3_LED    7
#define ERROR_LED  6
```

Status:

```text
Recommended as v1 baseline.
```

## 3. SUE ESP32-S3 Node

Source reference:

```text
sumkindafreak/SHOWDUINO---SUE
```

Target:

```text
ESP32-S3 XH3SE / SUE-style board
```

### FastLED

```cpp
#define LED_PIN   48
#define LED_COUNT 1
```

### SD Card

```cpp
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12
#define SD_CS   14
```

### RTC I2C

```cpp
#define RTC_SDA 8
#define RTC_SCL 9
#define RTC_SQ  10
```

### Relay / PWM Outputs

```cpp
#define RELAY_OUT1_PIN 1
#define RELAY_OUT2_PIN 2
#define RELAY_PWM1_PIN 3
#define RELAY_PWM2_PIN 4
```

### PCM5102A I2S Audio

```cpp
#define I2S_BCLK 5
#define I2S_WS   6
#define I2S_DOUT 7
```

Status:

```text
Imported from SUE modular config. Must be tested against actual hardware before final v1 release.
```

## 4. R3 Terminal Add-on

Source reference:

```text
User-confirmed R3 design notes
```

Target:

```text
ESP32 add-on puzzle terminal
```

### Pots

```cpp
#define POT1_PIN 27
#define POT2_PIN 26
#define POT3_PIN 25
```

### Relays

```cpp
#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define RELAY3_PIN 18
```

### NeoPixels

```cpp
#define PIXEL_PIN 33
#define NUM_PIXELS 6
```

### Buttons

```cpp
#define RESET_BUTTON_PIN   32
#define CONNECT_BUTTON_PIN 35
```

### DFPlayer Pro

```cpp
#define DFPLAYER_RX 16
#define DFPLAYER_TX 17
```

### Showduino Serial

```cpp
#define SHOWDUINO_RX 19
#define SHOWDUINO_TX 23
```

Status:

```text
Known project design. Add after main v1 baseline works.
```

## 5. Power Notes

Showduino will likely use mixed voltage rails:

- 5V for logic modules, some relays, pixels, audio modules
- 12V for higher-power relays, lights, props, solenoids, audio amps
- 3.3V for ESP32 logic

Rules:

- All grounds must be common where signals cross between boards.
- ESP32 GPIO is 3.3V logic and must not receive 5V directly.
- Use level shifting where required.
- NeoPixel data should ideally use a resistor on the data line.
- High-current pixels need separate power injection.
- Relay boards must match their trigger logic.
- Emergency stop should remove or disable dangerous output power where possible.

## 6. Confirmation Status Labels

Use these labels in future pinout docs:

```text
DRAFT       = planned but not tested
TESTED      = tested once on bench
CONFIRMED   = repeated and reliable
DEPRECATED  = old design, do not use
```

## v1 Baseline Decision

The first Showduino v1 hardware baseline should be:

```text
CYD Controller → Serial → Arduino Mega Executor → 8 Relays
```

Only after that works should v1 add:

1. DMX
2. MP3/audio
3. NeoPixels
4. SD show files
5. GoreFX dashboard
6. SUE wireless nodes
7. R3 terminal add-ons
