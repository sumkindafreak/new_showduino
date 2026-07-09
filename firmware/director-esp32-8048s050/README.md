# Showduino Portable Director 5in - ESP32-8048S050

Clean Showduino/GoreFX base firmware for the 5.0 inch ESP32-8048S050 ESP32-S3 RGB display.

This is now designed as a **portable detachable Showduino controller**:

```text
5in ESP32-S3 Director  ->  ESP-NOW  ->  P4 board built-in ESP32-C6 bridge  ->  UART/internal link  ->  ESP32-P4 Stage Engine
```

UART from the 5in Director is kept only as a bench/service fallback.

## What this firmware includes

- 800x480 ST7262 RGB display via Arduino_GFX
- GT911 capacitive touch
- LVGL 9 UI shell
- SPI SD card initialisation
- ESP-NOW portable controller transport
- UART fallback/service mode
- Emergency stop / clear buttons
- Live control, show library, diagnostics and settings screens
- Serial Monitor debug bridge

## Folder layout

```text
firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
  ShowduinoDirector8048S050.ino
  BoardConfig.h
  TouchDriver.h
  ShowduinoUi.h
  EspNowTransport.h

firmware/p4-c6-espnow-bridge/ShowduinoP4C6EspNowBridge/
  ShowduinoP4C6EspNowBridge.ino
```

## Arduino IDE settings for the 5in Director

Try these first:

- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- CPU Frequency: `240MHz`
- Flash Size: `16MB`
- Flash Mode: `QIO 80MHz`
- PSRAM: `OPI PSRAM` / `Enabled`
- Partition Scheme: `16M Flash` or `Huge APP`
- Serial Monitor: `115200 baud`

## Required Director libraries

Install these in Arduino IDE Library Manager:

- `lvgl` 9.x
- `Arduino_GFX_Library`
- `TAMC_GT911`

The ESP32 core provides `SPI`, `SD`, `FS`, `Wire`, `WiFi`, and ESP-NOW.

## Important pin notes

The manufacturer SDK uses GPIO19/GPIO20 for GT911 touch I2C. Because of that, the old Stage UART pins must not use 19/20 on this 5in board.

Service UART fallback in `BoardConfig.h`:

- RX: GPIO44
- TX: GPIO43
- Baud: 115200

The live control path should be ESP-NOW to the P4 board's built-in C6.

## ESP-NOW pairing flow

1. Upload `ShowduinoP4C6EspNowBridge.ino` to the built-in ESP32-C6 side of the P4 board.
2. Open Serial Monitor at 115200.
3. Copy the printed C6 MAC address.
4. Paste that MAC into the Director `BoardConfig.h` fields:

```cpp
#define SHOWDUINO_P4_C6_MAC_0 0xFF
#define SHOWDUINO_P4_C6_MAC_1 0xFF
#define SHOWDUINO_P4_C6_MAC_2 0xFF
#define SHOWDUINO_P4_C6_MAC_3 0xFF
#define SHOWDUINO_P4_C6_MAC_4 0xFF
#define SHOWDUINO_P4_C6_MAC_5 0xFF
```

5. Upload the 5in Director firmware.
6. Button commands should now travel wirelessly to the C6 bridge.

## SDK-derived hardware map

- LCD: ST7262 RGB, 800x480
- Backlight: GPIO2
- Touch: GT911, SDA GPIO19, SCL GPIO20, RST GPIO38
- SD: CS GPIO10, MOSI GPIO11, SCK GPIO12, MISO GPIO13
- Audio I2S documented ready: DOUT GPIO17, BCLK GPIO0, LRC GPIO18

## First Director test

1. Open `ShowduinoDirector8048S050.ino` in Arduino IDE.
2. Check the board settings above.
3. Upload.
4. Open Serial Monitor at 115200.
5. You should see the Showduino OS UI and touchable buttons.
6. Type `HELP` into Serial Monitor for bench commands.

## Next build step

The next sensible upgrade is to add reliable ACK/retry packets from the C6 bridge back to the portable Director, then add the real SD show browser for:

- `/projects`
- `/shows`
- `/scenes`
- `/assets`

Then the UI can load `.shdo` show files directly from the 5 inch Director.
