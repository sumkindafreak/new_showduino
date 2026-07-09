# Showduino Director 5in - ESP32-8048S050

Clean Showduino/GoreFX base firmware for the 5.0 inch ESP32-8048S050 ESP32-S3 RGB display.

This is not a raw manufacturer demo dump. It extracts the useful hardware details from the SDK and builds a proper Showduino foundation:

- 800x480 ST7262 RGB display via Arduino_GFX
- GT911 capacitive touch
- LVGL 9 UI shell
- SPI SD card initialisation
- Stage Engine UART command bridge
- Emergency stop / clear buttons
- Live control, show library, diagnostics and settings screens
- Serial Monitor debug bridge

## Arduino IDE settings

Try these first:

- Board: `ESP32S3 Dev Module`
- USB CDC On Boot: `Enabled`
- CPU Frequency: `240MHz`
- Flash Size: `16MB`
- Flash Mode: `QIO 80MHz`
- PSRAM: `OPI PSRAM` / `Enabled`
- Partition Scheme: `16M Flash` or `Huge APP`
- Serial Monitor: `115200 baud`

## Required libraries

Install these in Arduino IDE Library Manager:

- `lvgl` 9.x
- `Arduino_GFX_Library`
- `TAMC_GT911`

The ESP32 core provides `SPI`, `SD`, `FS`, and `Wire`.

## Important pin notes

The manufacturer SDK uses GPIO19/GPIO20 for GT911 touch I2C. Because of that, the Showduino Stage Engine UART has been moved away from 19/20.

Default Stage UART in `BoardConfig.h`:

- RX: GPIO44
- TX: GPIO43
- Baud: 115200

Change these if your wiring needs different free pins.

## SDK-derived hardware map

- LCD: ST7262 RGB, 800x480
- Backlight: GPIO2
- Touch: GT911, SDA GPIO19, SCL GPIO20, RST GPIO38
- SD: CS GPIO10, MOSI GPIO11, SCK GPIO12, MISO GPIO13
- Audio I2S documented ready: DOUT GPIO17, BCLK GPIO0, LRC GPIO18

## First test

1. Open `ShowduinoDirector8048S050.ino` in Arduino IDE.
2. Check the board settings above.
3. Upload.
4. Open Serial Monitor at 115200.
5. You should see the Showduino OS UI and touchable buttons.
6. Type `HELP` into Serial Monitor for bench commands.

## Next build step

The next sensible upgrade is to add the real SD show browser for:

- `/projects`
- `/shows`
- `/scenes`
- `/assets`

Then the UI can load `.shdo` show files directly from the 5 inch Director.
