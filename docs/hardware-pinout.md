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
GPIO25 -> momentary push button -> GND
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

### Current firmware conflict to remove

The existing Arduino Stage Controller sketch currently defines `STATUS_LED_PIN` as **GPIO10**. GPIO10 is the onboard ES8311 LRCK/WS line, so it is **not a valid general-purpose status LED pin** under the final hardware baseline.

That legacy status-LED assignment must be disabled/removed before the onboard audio path is enabled.

### Important I2S resource note

The ESP32-P4 provides one I2S peripheral. The onboard ES8311 and external PCM5102A are separate physical output paths but must be treated as a shared/arbitrated I2S resource until simultaneous operation is deliberately proven.

## 6. Onboard ESP32-C6 — BOARD / TARGET

The Communications Engine hardware moves to the ESP32-C6 already integrated into the Waveshare P4 module.

### Internal P4/C6 SDIO link

The module uses the standard ESP32-P4 / ESP32-C6 SDIO wiring:

```text
Function          P4 GPIO
C6 SDIO D0        14
C6 SDIO D1        15
C6 SDIO D2        16
C6 SDIO D3        17
C6 SDIO CLK       18
C6 SDIO CMD       19
C6 CHIP_PU/reset  54
```

**Reserved whenever the onboard C6 is active:** GPIO14-19 and GPIO54.

Do not invent exposed P4 UART GPIOs as the final product transport. The module separately exposes C6 UART pins for flashing/debugging.

### First qualification

Use:

```text
firmware/p4-c6-espnow-bridge/hosted-link-qualification/
```

This runs on the P4, preserves the factory C6 firmware, brings up the SDIO hosted link, reads the C6 STA MAC and performs a Wi-Fi scan.

### Current compatibility conflict

The current Arduino Stage Controller firmware still uses:

```text
P4 RX GPIO18 <- external C3 TX
P4 TX GPIO17 -> external C3 RX
baud 115200
```

Those pins are also the **onboard C6 SDIO CLK and D3 lines**. Therefore the old external-C3 UART and onboard-C6 SDIO transport are mutually exclusive firmware modes.

The old UART mapping remains a rollback implementation only. It must be removed or feature-gated before the main Show Engine enables the onboard C6 SDIO path.

### ESP-NOW software note

The factory C6 ESP-Hosted firmware supports the normal hosted Wi-Fi/Bluetooth path, but the standard host API does not currently expose ESP-NOW. Showduino therefore needs a deliberate C6-side ESP-NOW extension/custom service for Director/node traffic while retaining the internal transport.

### C6 programming interface

The module exposes:

```text
C6_U0RXD
C6_U0TXD
C6_IO9    download/boot control
```

Waveshare's C6 programming flow uses C6_IO9 to enter download mode and the C6 UART for flashing/debugging.

**Do not confuse `C6_IO9` with P4 GPIO9.** P4 GPIO9 is part of the onboard audio bus.

The factory C6 firmware/recovery path must be bench-qualified before replacing it.

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

## 9. Resource reservation summary

Do not allocate these P4 pins to unrelated new features under the current baseline:

```text
GPIO0-1    RTC 32.768 kHz crystal path
GPIO7-13   onboard I2C/I2S audio codec
GPIO14-19  onboard C6 SDIO transport
GPIO20-22  external PCM5102A show audio
GPIO24     emergency NeoPixel
GPIO25     emergency button
GPIO39-45  microSD / SDMMC + power
GPIO53     onboard speaker amplifier enable
GPIO54     onboard C6 CHIP_PU/reset
```

Additional board-integrated Ethernet, USB, camera/display and boot resources are also reserved even where they are not listed as ordinary Showduino GPIO outputs.

## 10. Current pin plan at a glance

```text
GPIO0-1  - RTC crystal path

GPIO7    - onboard audio I2C SDA
GPIO8    - onboard audio I2C SCL
GPIO9    - onboard audio DSDIN
GPIO10   - onboard audio LRCK   [NOT status LED]
GPIO11   - onboard audio ASDOUT
GPIO12   - onboard audio SCLK
GPIO13   - onboard audio MCLK

GPIO14   - onboard C6 SDIO D0
GPIO15   - onboard C6 SDIO D1
GPIO16   - onboard C6 SDIO D2
GPIO17   - onboard C6 SDIO D3
GPIO18   - onboard C6 SDIO CLK
GPIO19   - onboard C6 SDIO CMD

GPIO20   - PCM5102A WS/LRCK
GPIO21   - PCM5102A BCLK
GPIO22   - PCM5102A DOUT
GPIO24   - emergency NeoPixel
GPIO25   - emergency push button

GPIO39   - SD D0
GPIO40   - SD D1
GPIO41   - SD D2
GPIO42   - SD D3
GPIO43   - SD CLK
GPIO44   - SD CMD
GPIO45   - SD power

GPIO53   - onboard audio PA enable
GPIO54   - onboard C6 CHIP_PU/reset
```

## 11. Primary references

- Waveshare board documentation: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
- Waveshare C6 flashing FAQ: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ
- Waveshare module schematic / pinout
- Espressif ESP-Hosted P4/C6 SDIO documentation
- Espressif P4 RTC/VBAT documentation
