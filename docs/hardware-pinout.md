# Showduino Stage Controller Hardware Pin / Resource Map

Target board: **Waveshare ESP32-P4-Module-DEV-KIT**.

Canonical live command path:

```text
Director ESP32-S3 → ESP-NOW → dedicated ESP32-S3 Comms Controller
                    → UART (P4 GPIO4/5) → ESP32-P4 Show Engine
```

The onboard C6 is unused/reserved.

## 1. Dedicated S3 Comms UART — CURRENT

```text
P4 GPIO4  RX  <-  S3 GPIO17 TX
P4 GPIO5  TX  ->  S3 GPIO18 RX
115200 8N1, newline-framed ASCII
```

## 2. Onboard microSD — CURRENT / BOARD

```text
CLK       GPIO43
CMD       GPIO44
D0        GPIO39
D1        GPIO40
D2        GPIO41
D3        GPIO42
POWER     GPIO45   active LOW
```

The P4 firmware also configures the board LDO domain required by these pins.

## 3. Physical emergency button — CURRENT

```text
GPIO25 -> momentary pushbutton -> GND
INPUT_PULLUP
released = HIGH
pressed  = LOW
30 ms debounce
```

The input triggers a latched emergency. Release alone does not clear it.

## 4. Emergency / designated-signage NeoPixel line — CURRENT

```text
DATA = GPIO24
configured maximum = 100 pixels
1 sign bundle = 10 pixels
```

The line is safety-owned and is not an ordinary show-output lane.

Normal behaviour is generated automatically:

```text
pixels 0-9:    pixel 0 GREEN, 1-9 OFF
pixels 10-19:  pixel 10 GREEN, 11-19 OFF
pixels 20-29:  pixel 20 GREEN, 21-29 OFF
...
```

During a Showduino emergency, **every pixel on GPIO24 becomes bright white in one prepared frame** so all sign bundles change together.

On authorised clear the GPIO24 line returns to the green-locator pattern.

## 5. Local theatrical Show Pixel line — CURRENT / HARDWARE TEST REQUIRED

```text
DATA = GPIO23
configured default = 100 pixels
maximum segment slots = 16
engine update target = 20 ms
```

GPIO23 is the only local general-purpose P4 show-pixel line in this hardware generation. Additional theatrical strips belong on specialist Pixel Nodes.

The P4 firmware implements independent segments and the shared 25-effect Showduino FX vocabulary. See [`audio-pixel-engine.md`](audio-pixel-engine.md).

Hard emergency policy:

```text
NORMAL     → segmented show FX
EMERGENCY  → ALL GPIO23 PIXELS BRIGHT WHITE
CLEAR      → BLACKOUT; interrupted FX do not auto-resume
```

## 6. P4 pixel wiring standard

Each P4 pixel data line gets its own **470 Ω series resistor**:

```text
GPIO23 / 5V buffer ── 470 Ω ──> Show Pixel DIN
GPIO24 / 5V buffer ── 470 Ω ──> Emergency/Signage DIN
```

Place the resistor close to the P4-side driver/logic buffer.

For final installations or longer data cables, use a 5 V-compatible buffer such as a **74AHCT125 / 74HCT125-class** device:

```text
P4 3.3V GPIO → 5V logic buffer → 470 Ω → NeoPixel DIN
```

The resistor is signal conditioning; it is **not** a logic-level converter.

Also:

- P4 ground and pixel PSU ground must be common.
- Pixels should use an appropriately sized external 5 V supply.
- Add roughly 1000 µF bulk capacitance across 5 V/GND near the start of a substantial line.
- Provide power injection appropriate to total pixel current and cable length.

## 7. External PCM5102A — LEGACY / RETIRED

The old external I2S DAC on GPIO20/21/22 is not a live Showduino path.

```text
GPIO20  legacy WS/LRCK
GPIO21  legacy BCLK
GPIO22  legacy P4 DOUT
```

Attraction/programme audio belongs to the specialist Audio Node. Do not reassign GPIO20-22 without a fresh pin audit.

## 8. Onboard ES8311 system audio — CURRENT

The onboard ES8311 provides **Showduino system/safety audio only**.

```text
I²C address      0x18
SDA              GPIO7     shared Plug-in Bus
SCL              GPIO8     shared Plug-in Bus
DSDIN            GPIO9     P4 I2S DOUT → codec DAC
LRCK / WS        GPIO10    NOT a status LED
ASDOUT           GPIO11    codec ADC out, unused for playback
SCLK / BCLK      GPIO12
MCLK             GPIO13
PA_Ctrl          GPIO53    NS4150B enable, active HIGH
```

`SHOWDUINO_STATUS_LED_PIN` is `-1`. GPIO10 must not be driven as a generic LED.

## 9. Showduino Plug-in Bus — CURRENT

```text
SDA = GPIO7
SCL = GPIO8
100 kHz
3.3 V logic
```

The bus shares the board I²C lines with the onboard ES8311. Do not add 5 V I²C pull-ups.

## 10. Internal ESP32-P4 RTC — CURRENT

No DS3231 is used in this generation.

GPIO0/GPIO1 are the board 32.768 kHz RTC crystal path and remain reserved.

## 11. Onboard ESP32-C6 — UNUSED / RESERVED

Do not allocate:

```text
GPIO6      onboard C6 control
GPIO14     C6 SDIO D0
GPIO15     C6 SDIO D1
GPIO16     C6 SDIO D2
GPIO17     C6 SDIO D3
GPIO18     C6 SDIO CLK
GPIO19     C6 SDIO CMD
GPIO54     C6 CHIP_PU/reset
```

The dedicated external ESP32-S3 is the live Communications Engine. Do not revive P4 GPIO17/18 as an external UART.

## 12. Ethernet — BOARD / CURRENT FOUNDATION

Waveshare onboard PHY: IP101GRI over RMII.

```text
TX_EN      GPIO49
TXD0       GPIO34
TXD1       GPIO35
RXD0       GPIO29
RXD1       GPIO30
CRS_DV     GPIO28
REF_CLK    GPIO50
MDC        GPIO31
MDIO       GPIO52
RESET      GPIO51
PHY addr   1
```

Ethernet remains optional to show operation. No cable, DHCP or internet is required for local show execution.

DMX/E1.31-related development remains parked until explicitly reopened.

## 13. P4 pin plan at a glance

```text
GPIO0-1    RTC crystal path
GPIO4      S3 Comms UART RX
GPIO5      S3 Comms UART TX
GPIO6      onboard C6 control — reserved unused
GPIO7      Plug-in Bus / ES8311 I2C SDA
GPIO8      Plug-in Bus / ES8311 I2C SCL
GPIO9      ES8311 DSDIN
GPIO10     ES8311 LRCK / WS — NOT status LED
GPIO11     ES8311 ASDOUT
GPIO12     ES8311 BCLK
GPIO13     ES8311 MCLK
GPIO14-19  onboard C6 SDIO — reserved unused
GPIO20-22  unused; legacy PCM5102A retired
GPIO23     CURRENT local segmented Show Pixel line
GPIO24     CURRENT emergency/designated-signage pixel line
GPIO25     emergency pushbutton
GPIO28-31  Ethernet
GPIO34-35  Ethernet
GPIO39-45  SDMMC + power
GPIO49-52  Ethernet
GPIO53     onboard audio PA enable
GPIO54     onboard C6 CHIP_PU/reset
```

## 14. Specialist Audio Node pin summary

This is not P4 wiring. Canonical detail: [`audio-node.md`](audio-node.md).

```text
I2C SDA/SCL     GPIO33 / GPIO32
I2S MCLK        GPIO0
I2S BCLK/LRCK   GPIO27 / GPIO25
I2S DOUT/DIN    GPIO26 / GPIO35
PA enable       GPIO21
SD SPI          14 / 2 / 15 / 13
SD detect       GPIO34
HP detect       GPIO39
Status LED      GPIO22
```

## 15. C3 Pixel Node — CURRENT / HARDWARE TEST REQUIRED

ESP32-C3 Super Mini OLED. Same board family as the Lamp Node.

```text
OLED SDA      GPIO5
OLED SCL      GPIO6
OLED addr     0x3C
Pixel DATA    GPIO2
Buttons       GPIO9 (A / BOOT), GPIO0 (B)
```

Do not move OLED pins. Do not power the strip from the C3. See [`c3-pixel-node.md`](c3-pixel-node.md) and [`firmware/c3-pixel-node/README.md`](../firmware/c3-pixel-node/README.md).

## 16. Related documents

- [`final-hardware-architecture.md`](final-hardware-architecture.md)
- [`audio-pixel-engine.md`](audio-pixel-engine.md)
- [`node-roadmap.md`](node-roadmap.md)
- Waveshare board documentation: https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT
