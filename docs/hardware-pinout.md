# Showduino Stage Controller Hardware Pin / Resource Map

Target board: **Waveshare ESP32-P4-Module-DEV-KIT**.

This replaces the old CYD/Mega/SUE-era pinout as the canonical Stage Controller resource document.

## Status labels

```text
BOARD       = fixed by the Waveshare board design/documentation
CURRENT     = used by current Showduino firmware/wiring
TARGET      = selected for Showduino but not yet qualified in final firmware
RESERVED    = do not allocate to unrelated features
COMPAT      = retained only for previous/rollback hardware
```

## 1. Onboard microSD — CURRENT / BOARD

The Stage Controller uses the board's SDMMC interface, not an SPI SD breakout.

```text
Function  P4 GPIO
CLK       43
CMD       44
D0        39
D1        40
D2        41
D3        42
POWER     45   active LOW
```

`BoardConfig.h` also configures the P4 LDO domain required by these pins.

**Reserved:** GPIO39-45.

## 2. Physical emergency button — CURRENT

```text
GPIO25 → momentary push button → GND
INPUT_PULLUP
released = HIGH
pressed  = LOW
30 ms debounce
```

GPIO25 is a trigger only. The P4 latches the emergency state in software.

**Reserved:** GPIO25.

## 3. Emergency NeoPixel line — CURRENT

```text
DATA = GPIO24
```

The current Stage Controller firmware owns this as the emergency pixel line.

**Reserved:** GPIO24.

## 4. External PCM5102A show-audio DAC — CURRENT

Dedicated to show/programme audio.

```text
PCM5102A        P4 GPIO
WS / LRCK       20
BCLK            21
DIN (P4 DOUT)   22
```

This path is for music, dialogue, ambience and timed show SFX.

**Reserved:** GPIO20-22.

## 5. Onboard ES8311 system audio — BOARD / TARGET

The Waveshare board integrates an ES8311 codec and NS4150B power amplifier.

### Control I2C

```text
SDA  GPIO7
SCL  GPIO8
```

### I2S

```text
ES8311 function  P4 GPIO
DSDIN            9
LRCK / WS        10
ASDOUT           11
SCLK / BCLK      12
MCLK             13
PA enable        53   active HIGH
```

Showduino reserves this path for local system sounds: boot, ready, loaded, armed, warning/error, emergency acknowledgement and restart/shutdown cues.

**Reserved:** GPIO7-13 and GPIO53.

### Important I2S resource note

The ESP32-P4 provides one I2S peripheral. The onboard ES8311 and external PCM5102A are separate physical output paths but must be treated as a shared/arbitrated I2S resource until simultaneous operation is deliberately proven.

## 6. Onboard ESP32-C6 — BOARD / TARGET

The Communications Engine hardware moves to the ESP32-C6 already integrated into the Waveshare P4 module.

### Normal P4/C6 relationship

The module uses an internal **SDIO** link for C6 Wi-Fi/Bluetooth expansion. Do not invent exposed P4 UART GPIOs as the final product transport.

The current `firmware/p4-c6-espnow-bridge/` sketch contains old placeholder UART assumptions and must be treated as bring-up code only.

### C6 programming interface

The module exposes:

```text
C6_U0RXD
C6_U0TXD
C6_IO9    download/boot control
```

Waveshare flashing procedure:

1. Pull `C6_IO9` LOW during power/reset.
2. Put the P4 into download mode as required by the board procedure.
3. Flash through `C6_U0RXD` / `C6_U0TXD`.

**Do not confuse `C6_IO9` with P4 GPIO9.** P4 GPIO9 is part of the onboard audio bus.

The factory C6 firmware/recovery path must be understood before replacing it.

## 7. ESP32-P4 RTC / VBAT — BOARD / TARGET

The final Stage Controller does not use a DS3231 module.

The P4 provides an RTC domain and the Waveshare board exposes a rechargeable RTC battery connection.

The P4 module also includes the 32.768 kHz RTC crystal path on P4 GPIO0/GPIO1. Treat those pins as reserved by the board RTC design unless the hardware design is intentionally changed.

```text
External DS3231: REMOVED
RTC battery header: rechargeable cells only per Waveshare documentation
```

No ordinary Showduino GPIO assignment is required for the RTC timekeeper.

## 8. Ethernet and USB — BOARD / RESERVED

Ethernet and USB are board-integrated Stage Controller resources.

Do not repurpose their board-level signals based only on a generic ESP32-P4 pin table. Check the Waveshare schematic before allocating any pin that may be consumed by the Ethernet PHY, USB routing, boot circuitry or other onboard functions.

## 9. Previous external C3 UART — COMPATIBILITY ONLY

The current Stage Controller code still contains the previous external-C3 UART mapping:

```text
P4 RX GPIO18 ← external C3 TX
P4 TX GPIO17 → external C3 RX
baud 115200
```

This is retained for firmware migration/rollback and is **not** part of the final physical Stage Controller baseline.

GPIO17/18 may only be released for another use after the onboard C6 transport fully replaces this compatibility path in firmware.

## 10. Resource reservation summary

Do not allocate these P4 pins to unrelated new features under the current baseline:

```text
GPIO0-1    RTC 32.768 kHz crystal path
GPIO7-13   onboard I2C/I2S audio codec
GPIO17-18  temporary external-C3 compatibility UART
GPIO20-22  external PCM5102A show audio
GPIO24     emergency NeoPixel
GPIO25     emergency button
GPIO39-45  microSD / SDMMC + power
GPIO53     onboard speaker amplifier enable
```

Additional board-integrated Ethernet, USB, C6/SDIO, camera/display and boot resources are also reserved even where they are not listed as ordinary Showduino GPIO outputs.

## 11. Current pin plan at a glance

```text
GPIO7  ─ onboard audio I2C SDA
GPIO8  ─ onboard audio I2C SCL
GPIO9  ─ onboard audio DSDIN
GPIO10 ─ onboard audio LRCK
GPIO11 ─ onboard audio ASDOUT
GPIO12 ─ onboard audio SCLK
GPIO13 ─ onboard audio MCLK

GPIO17 ─ compatibility UART TX to old external C3
GPIO18 ─ compatibility UART RX from old external C3

GPIO20 ─ PCM5102A WS/LRCK
GPIO21 ─ PCM5102A BCLK
GPIO22 ─ PCM5102A DOUT
GPIO24 ─ emergency NeoPixel
GPIO25 ─ emergency push button

GPIO39 ─ SD D0
GPIO40 ─ SD D1
GPIO41 ─ SD D2
GPIO42 ─ SD D3
GPIO43 ─ SD CLK
GPIO44 ─ SD CMD
GPIO45 ─ SD power
GPIO53 ─ onboard audio PA enable
```

## 12. Primary references

- Waveshare board documentation: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
- Waveshare C6 flashing FAQ: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ
- Waveshare module pinout: https://www.waveshare.com/wiki/ESP32-P4-Module
- Espressif P4 RTC/VBAT notes: https://docs.espressif.com/projects/esp-iot-solution/en/latest/low_power_solution/esp32p4_vbat.html
