# Showduino Stage Controller Hardware Pin / Resource Map

Target board: **Waveshare ESP32-P4-Module-DEV-KIT**.

This replaces the old CYD/Mega/SUE-era pinout as the canonical Stage Controller resource document.

Current live command path is **Director → ESP-NOW → dedicated ESP32-S3 Comms Controller → UART (P4 GPIO4/5) → P4**. The onboard C6 is unused/reserved. Topology: [`final-hardware-architecture.md`](final-hardware-architecture.md).

## Status labels

```text
BOARD       = fixed by the Waveshare board design/documentation
CURRENT     = used by current Showduino firmware/wiring
TARGET      = selected for Showduino but not yet qualified in final firmware
RESERVED    = do not allocate to unrelated features
COMPAT      = retained only for previous/rollback hardware
```

## 0. Dedicated S3 Comms UART — CURRENT

```text
P4 GPIO4  RX  <-  S3 GPIO17 TX
P4 GPIO5  TX  ->  S3 GPIO18 RX
115200 8N1, newline-framed ASCII
```

**Reserved:** GPIO4, GPIO5. GPIO6 remains reserved for onboard C6 control and must not be used as UART.

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

## 4. External PCM5102A — LEGACY / RETIRED

The old external I2S DAC on GPIO20/21/22 is **not** a live Showduino path.

```text
LEGACY PCM5102A   P4 GPIO
WS / LRCK         20
BCLK              21
DIN (P4 DOUT)     22
```

That driver reported WAV "playing" while the onboard speaker stayed silent. Firmware compiles it out (`SHOWDUINO_LEGACY_PCM5102A_AUDIO 0`). Attraction/programme audio is exclusively the specialist Audio Node. GPIO20-22 are unused; do not reassign them without a pin audit.

## 5. Onboard ES8311 system audio — CURRENT

The P4 onboard ES8311 speaker provides Showduino system and safety audio only. Attraction/programme audio is exclusively produced by specialist Audio Nodes.

Verified from the Waveshare ESP32-P4-Module-DEV-KIT wiki, schematic names, ESP-IDF P4 ES8311 examples, and Arduino-ESP32 3.3.11 `waveshare_p4_poe_eth` notes.

```text
I²C address      0x18
SDA              GPIO7     shared Plug-in Bus
SCL              GPIO8     shared Plug-in Bus
DSDIN            GPIO9     ES8311 DAC in  = P4 I2S DOUT
LRCK / WS        GPIO10    not a status LED
ASDOUT           GPIO11    ES8311 ADC out = P4 I2S DIN (unused for playback)
SCLK / BCLK      GPIO12
MCLK             GPIO13    required (sample_rate × 256)
PA_Ctrl          GPIO53    NS4150B enable, active HIGH
Speaker          MX1.25 2P  8Ω / 2W Waveshare speaker connector
```

Arduino-ESP32 3.3.11 `waveshare_p4_poe_eth/pins_arduino.h` labels `I2S_DOUT=11` / `I2S_DIN=9`. Those names are swapped versus the Waveshare wiki and working ESP-IDF P4 examples. Showduino uses the wiki mapping (MCU DOUT=GPIO9).

`SHOWDUINO_STATUS_LED_PIN` is `-1`. GPIO10 must not be driven as a generic LED.

**Reserved:** GPIO7-13 and GPIO53.

## 6. Onboard ESP32-C6 — BOARD / UNUSED / RESERVED

The Waveshare module includes an ESP32-C6. **Showduino application firmware does not currently use it.** Current Communications Engine hardware is the dedicated ESP32-S3. Keep these nets reserved; do not flash the onboard C6 for the live product path.

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

**Reserved (do not allocate):** GPIO6, GPIO14-19 and GPIO54.

Do not invent exposed P4 UART GPIOs as the final product transport. The module separately exposes C6 UART pins for flashing/debugging.

### First qualification

Use:

```text
firmware/p4-c6-espnow-bridge/hosted-link-qualification/
```

This runs on the P4, preserves the factory C6 firmware, brings up the SDIO hosted link, reads the C6 STA MAC and performs a Wi-Fi scan.

### Do not reuse C6 SDIO pins for UART

Live Showduino UART is **P4 GPIO4/5** to the dedicated S3 Comms Controller. Do not revive the old external-C3 UART on GPIO17/18: those pins are onboard C6 SDIO D3/CLK and stay reserved.

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

## 8. Ethernet — BOARD / CURRENT

Waveshare ESP32-P4-Module-DEV-KIT onboard PHY is **IP101GRI** on RMII. Arduino-ESP32 3.3.11 uses `ETH_PHY_TLK110` / `ETH_PHY_IP101`.

```text
Function   P4 GPIO
TX_EN      49
TXD0       34
TXD1       35
RXD0       29
RXD1       30
CRS_DV     28
REF_CLK    50   50 MHz from PHY, EMAC_CLK_EXT_IN
MDC        31
MDIO       52
RESET      51
PHY addr   1
```

These nets do not collide with Comms UART, Plug-in Bus, onboard ES8311, emergency pixels, GPIO25, SDMMC, or reserved C6 pins.

Ethernet is optional. The Show Engine boots and runs with no cable, no DHCP, and no internet. Do not treat internet reachability as system health.

USB remains a board-integrated programming/debug resource. Do not steal it for show control.

## 8a. Local NeoPixel baseline

```text
GPIO24  CURRENT  dedicated emergency NeoPixel line
GPIO23  TARGET   the only planned general-purpose Show NeoPixel line
```

Do not implement additional local P4 show-pixel outputs. Extra strips belong to future Pixel / LED Nodes.

## 9. Resource reservation summary

Do not allocate these P4 pins to unrelated new features under the current baseline:

```text
GPIO0-1    RTC 32.768 kHz crystal path
GPIO4-5    dedicated S3 Comms UART
GPIO6      onboard C6 control (reserved unused)
GPIO7-13   onboard I2C/I2S audio codec + Plug-in Bus SDA/SCL
GPIO14-19  onboard C6 SDIO transport (reserved unused)
GPIO20-22  unused (legacy PCM5102A — retired)
GPIO23     planned local Show NeoPixel line
GPIO24     emergency NeoPixel
GPIO25     emergency button
GPIO28-31, 34-35, 49-52  onboard Ethernet RMII / SMI
GPIO39-45  microSD / SDMMC + power
GPIO53     onboard speaker amplifier enable
GPIO54     onboard C6 CHIP_PU/reset
```

Additional board-integrated Ethernet, USB, camera/display and boot resources are also reserved even where they are not listed as ordinary Showduino GPIO outputs.

## 10. Current pin plan at a glance

```text
GPIO0-1  - RTC crystal path
GPIO4    - S3 Comms UART RX
GPIO5    - S3 Comms UART TX
GPIO6    - onboard C6 control (reserved unused)

GPIO7    - Plug-in Bus / onboard audio I2C SDA
GPIO8    - Plug-in Bus / onboard audio I2C SCL
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

GPIO20   - unused (legacy PCM5102A WS)
GPIO21   - unused (legacy PCM5102A BCLK)
GPIO22   - unused (legacy PCM5102A DOUT)
GPIO23   - planned general Show NeoPixel line (not implemented)
GPIO24   - emergency NeoPixel
GPIO25   - emergency push button

GPIO28   - ETH CRS_DV
GPIO29   - ETH RXD0
GPIO30   - ETH RXD1
GPIO31   - ETH MDC
GPIO34   - ETH TXD0
GPIO35   - ETH TXD1
GPIO49   - ETH TX_EN
GPIO50   - ETH REF_CLK
GPIO51   - ETH PHY reset
GPIO52   - ETH MDIO

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

## 11. Audio Node — Ai-Thinker ESP32-Audio-Kit V2.2 A161 — CURRENT

This is **not** a P4 pin. Attraction/programme audio is the specialist Audio Node (ESP32-A1S + ES8388). Canonical detail: [`audio-node.md`](audio-node.md).

```text
I2C SDA/SCL     GPIO33 / GPIO32     ES8388 0x10 (fallback 0x11)
I2S MCLK        GPIO0               boot strap
I2S BCLK/LRCK   GPIO27 / GPIO25
I2S DOUT/DIN    GPIO26 / GPIO35
PA enable       GPIO21              HIGH = on
SD SPI          14 / 2 / 15 / 13    SCK / MISO / MOSI / CS
SD detect       GPIO34              LOW = present + probe
HP detect       GPIO39              LOW = jack; AUTO only
KEY1            GPIO36              local PLAY/STOP (input-only)
KEY2            disabled            shares SD CS
KEY3            GPIO19              VOL- (LED5 shares pin)
KEY4            GPIO23              VOL+
KEY5            GPIO18              previous test asset
KEY6            GPIO5               next test asset
KEY7            none
Status LED      GPIO22              LED4
```

P4 onboard ES8311 / GPIO53 PA remains system/safety audio only.

## 12. Primary references

- Waveshare board documentation: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
- Waveshare C6 flashing FAQ: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ
- Waveshare module schematic / pinout
- Espressif ESP-Hosted P4/C6 SDIO documentation
- Espressif P4 RTC/VBAT documentation
