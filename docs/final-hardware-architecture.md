# Showduino Hardware Architecture — current generation

Authoritative hardware topology and Stage Controller pin/resource baseline for the **Waveshare ESP32-P4-Module-DEV-KIT** generation.

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

---

## Current architecture

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
Dedicated ESP32-S3 Communications Engine
        │ UART 115200 8N1
        ▼
ESP32-P4 Show Engine / Stage Controller

Browser / tablet / laptop
        │ Wi-Fi
        ▼
Same S3 Communications Engine
        │ serves Showduino Studio from PROGMEM
        │ proxies API requests over UART
        ▼
P4 authoritative API/state

Specialist Node
        │ ESP-NOW
        ▼
S3 Communications Engine
        │ UART
        ▼
P4 Show Engine
```

Current active firmware:

```text
firmware/director-esp32-8048s050/      Director
firmware/s3-comms-controller/          Communications Engine + Studio host
firmware/stage-engine-p4/              Show Engine / Stage Controller
firmware/audio-node-esp32-a1s/         first specialist Audio Node
```

The P4 runs locally if the S3, Director, browser or Wi-Fi client disappears. GPIO25 emergency handling and P4-local safety outputs remain local to the P4.

The onboard ESP32-C6 is **unused/reserved hardware**. Do not require, erase or flash it for the current Showduino application path.

---

## Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Authoritative software/runtime role |
| **Stage Controller** | Physical ESP32-P4 product running the Show Engine |
| **Communications Engine** | Dedicated ESP32-S3 Dev Module |
| **Director** | ESP32-S3 touchscreen operator desk |
| **Audio Node** | ESP32-A1S/ES8388 programme-audio specialist |
| **C3 Lantern Node** | Next planned specialist Node after Audio commissioning |
| **C3 Pixel Node** | Planned after Lantern |
| **MOSFET Node** | Planned after C3 Pixel; supersedes Relay Node direction |
| ~~Stage Engine~~ | Retired role term; may remain in legacy path names |
| ~~Relay Node as next product~~ | Superseded by MOSFET Node direction |

DMX remains parked/out of scope until explicitly revisited.

---

## Current P4 pin map

| P4 GPIO | Function |
|---------|----------|
| 0-1 | RTC 32.768 kHz crystal path / reserved board function |
| 4 | Comms UART RX from S3 GPIO17 TX |
| 5 | Comms UART TX to S3 GPIO18 RX |
| 6 | **RESERVED** onboard C6 control |
| 7 | Plug-in Bus SDA + ES8311 I²C SDA |
| 8 | Plug-in Bus SCL + ES8311 I²C SCL |
| 9 | ES8311 DSDIN, P4 I2S DOUT |
| 10 | ES8311 LRCK/WS — **not a status LED** |
| 11 | ES8311 ASDOUT, unused for playback |
| 12 | ES8311 BCLK/SCLK |
| 13 | ES8311 MCLK |
| 14-19 | **RESERVED** onboard C6 SDIO |
| 20-22 | unused; legacy PCM5102A path retired |
| 23 | **CURRENT** P4 Main Show Pixel Line |
| 24 | **CURRENT** emergency/signage Pixel Line |
| 25 | physical emergency button, active LOW, software latch |
| 28-31, 34-35, 49-52 | onboard Ethernet RMII/SMI |
| 39-45 | onboard SDMMC microSD + power |
| 53 | NS4150B onboard speaker amplifier enable |
| 54 | **RESERVED** onboard C6 reset/CHIP_PU |

Firmware source of truth: `firmware/stage-engine-p4/ShowduinoStageEngineP4/BoardConfig.h`.

### Communications UART

```text
S3 GPIO17 TX  →  P4 GPIO4 RX
S3 GPIO18 RX  ←  P4 GPIO5 TX
GND shared
115200 8N1
newline-framed ASCII
```

S3 USB remains programming/debug. The Director is not normally connected to this UART.

---

## P4 pixel hardware

### GPIO23 — Main Show Pixel Line

GPIO23 is the only local general/theatrical NeoPixel line in this P4 generation.

Current firmware provides:

- non-blocking segmented rendering;
- up to 16 live segment slots;
- simultaneous different FX on different regions;
- shared 25-FX Showduino vocabulary;
- direct commissioning/status commands;
- global emergency-white override.

Hardware commissioning is still required before claiming the line physically proven on the current enclosure.

### GPIO24 — Emergency/signage line

GPIO24 is safety-owned. Current configuration supports up to 100 pixels in fixed 10-pixel emergency-sign bundles.

Normal per group:

```text
first pixel  GREEN
remaining 9  OFF
```

Emergency per group:

```text
all 10 pixels WHITE
```

All configured groups are composed into one frame and transmitted together so the designated signs change in sync.

### Global emergency pixel rule

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

The rule applies to:

- P4 GPIO23 Main Show Pixels;
- P4 GPIO24 emergency/signage pixels;
- future C3 Lantern pixels where fitted;
- future C3 Pixel Node outputs;
- every later Showduino pixel-capable output.

No segment, effect, production cue or UI control can defeat this override.

On clear, GPIO24 returns to the green-locator pattern. GPIO23 and future theatrical pixel outputs do not automatically resume interrupted FX.

### Pixel wiring standard

Each P4 pixel DATA line uses its own **470 Ω series resistor**.

Production wiring target:

```text
P4 GPIO23 → 5 V-capable logic buffer → 470 Ω → Main Pixel DIN
P4 GPIO24 → 5 V-capable logic buffer → 470 Ω → Emergency Pixel DIN
```

Use an external 5 V pixel supply sized for the installed load, common P4/pixel ground, and approximately 1000 µF bulk capacitance across 5 V/GND near the beginning of a substantial line.

A direct 3.3 V GPIO → 470 Ω → DIN connection is acceptable as a short bench experiment where it works, but the resistor is **not** a level shifter. Final installations should use a 5 V-capable buffer such as a 74AHCT125/74HCT125-class device.

Power/current design must allow for worst-case **all-white emergency load**.

---

## P4 onboard system/safety audio

The onboard ES8311 + NS4150B path is live for Showduino system/safety audio only.

```text
I2C address   0x18
SDA           GPIO7
SCL           GPIO8
I2S DOUT      GPIO9
LRCK/WS       GPIO10
I2S DIN       GPIO11 (unused for playback)
BCLK          GPIO12
MCLK          GPIO13
PA enable     GPIO53
```

Attraction/programme audio belongs exclusively to the specialist Audio Node. The old external PCM5102A GPIO20/21/22 path is retired.

---

## P4 SD storage

Onboard microSD uses SDMMC Slot 0:

```text
D0     GPIO39
D1     GPIO40
D2     GPIO41
D3     GPIO42
CLK    GPIO43
CMD    GPIO44
POWER  GPIO45 active LOW
```

The P4 SD is persistent production/configuration storage. It is not the safety backbone; boot/emergency/transport must degrade safely if the card is unavailable.

---

## Dedicated S3 Communications Engine

Firmware: `firmware/s3-comms-controller/`

Current responsibilities:

- ESP-NOW transport for Director and Nodes;
- UART transport to P4;
- remember/discover transport peers;
- Wi-Fi SoftAP for browser access;
- serve the canonical Showduino Studio static frontend from generated PROGMEM;
- proxy browser API requests to the P4.

It must not:

- run timelines;
- own production state;
- execute P4 pixel FX;
- claim remote action completion without a report;
- replace P4 authority.

BLE/OTA may remain future. Onboard P4 C6/ESP-Hosted is not part of this path.

### S3 WebUI source vs generated bundle

Canonical frontend source:

```text
web/showduino-studio/
```

Generated S3 asset bundle:

```text
firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h
```

After frontend source changes, regenerate the bundle with `tools/embed-webui/embed_webui.py` before expecting those changes in the flashed S3 UI.

---

## P4 Show Engine responsibilities

Firmware: `firmware/stage-engine-p4/`

Owns:

- authoritative runtime/show/emergency state;
- production discovery/loading from P4 SD;
- RAM timeline execution;
- P4 Web API/state origin;
- local ES8311 system/safety audio;
- GPIO23 segmented Show Pixel Engine;
- GPIO24 emergency/signage Pixel Engine;
- GPIO25 emergency input/latch policy;
- node routing/lifecycle authority;
- Plug-in Bus;
- optional Ethernet and isolated E1.31 test receiver.

Persistent production-format v1 currently accepts TEST/LOG timeline cues only. Direct pixel commands exist for commissioning/runtime control, but production `PIXEL` cue parsing/named segment persistence is not yet complete.

---

## Specialist Nodes

### Audio Node — current

Firmware: `firmware/audio-node-esp32-a1s/`

ESP32-A1S/ES8388 with local SD WAV playback. Implemented firmware; hardware commissioning required.

### C3 Lantern Node — next

Next specialist-node milestone after current Audio/P4 pixel commissioning. Do not alter its assembly/code until explicitly requested.

### C3 Pixel Node — after Lantern

Will reuse `protocol/showduino_pixel_fx.h` so P4 and C3 share the same Showduino FX names/parameters and emergency-white rule.

### MOSFET Node — after C3 Pixel

Supersedes the old Relay Node product direction.

### Legacy Relay prototype

`firmware/relay-node-esp32/` remains historical/experimental source only. It is not the next production Node.

---

## Onboard ESP32-C6 — unused/reserved

The Waveshare module contains an ESP32-C6 and internal P4↔C6 SDIO/control nets:

```text
P4 GPIO14-19  SDIO
P4 GPIO54     C6 reset/CHIP_PU
P4 GPIO6      C6 control
```

These remain reserved. Current Showduino application firmware does not depend on the C6, ESP-Hosted or custom C6 firmware.

---

## Ethernet

Onboard IP101GRI RMII resources remain board-reserved:

```text
TX_EN   49
TXD0    34
TXD1    35
RXD0    29
RXD1    30
CRS_DV  28
REF_CLK 50
MDC     31
MDIO    52
RESET   51
```

Ethernet is optional. E1.31 is currently an isolated test/observation receiver only. DMX/E1.31 production output/mapping remains parked.

---

## Minimum current bench system

```text
1 × P4 Stage Controller
1 × dedicated S3 Communications Engine
1 × Director ESP32-S3 touchscreen
1 × Audio Node for programme-audio commissioning
1 × GPIO23 NeoPixel bench strip
1 × GPIO24 emergency/signage test chain as needed
```

Critical safety tests include emergency operation with Director/browser absent and all pixel outputs forced white.

---

## Product-family direction

```text
Showduino Director
Showduino Stage Controller / Show Engine
Showduino Communications Engine
Showduino Audio Node
Showduino C3 Lantern Node
Showduino C3 Pixel Node
Showduino MOSFET Node
```

DMX is intentionally omitted until explicitly brought back into scope.

---

## Related documents

- [`architecture.md`](architecture.md)
- [`hardware-pinout.md`](hardware-pinout.md)
- [`audio-pixel-engine.md`](audio-pixel-engine.md)
- [`studio-pixel-authoring.md`](studio-pixel-authoring.md)
- [`production-storage.md`](production-storage.md)
- [`p4-ethernet-e131-test-foundation.md`](p4-ethernet-e131-test-foundation.md)
