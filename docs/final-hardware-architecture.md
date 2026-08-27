# Showduino Hardware Architecture (current generation)

Authoritative hardware topology and Stage Controller pin map for the **Waveshare ESP32-P4-Module-DEV-KIT** generation.

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

---

## Current architecture (locked)

```text
Director ESP32-S3
        |
        | ESP-NOW
        v
Dedicated ESP32-S3 Comms Controller
        |
        | UART 115200 8N1, newline-framed ASCII
        v
ESP32-P4 Stage Engine
```

Firmware:

```text
firmware/director-esp32-8048s050/          Director
firmware/s3-comms-controller/              Dedicated S3 communications controller
firmware/stage-engine-p4/                  Show Engine (P4)
firmware/relay-node-esp32/                 Relay Node (ESP-NOW via S3)
```

The Waveshare onboard ESP32-C6 is **UNUSED BY SHOWDUINO / RESERVED HARDWARE**.  
Do not require C6 firmware, ESP-NOW, SDIO, ESP-Hosted, or WebUI on the C6.  
Do not erase or flash the onboard C6. Its reserved nets stay reserved.

The factory internal P4↔C6 **SDIO** bus is **not** used for Showduino command transport.

The P4 boots and runs locally if the S3 Comms Controller is missing. GPIO25 emergency handling is entirely local to the P4.

---

## Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Software / processor role: single source of truth |
| **Stage Controller** | Physical **ESP32-P4** product that runs the Show Engine |
| **Communications Engine** | Dedicated **ESP32-S3 Dev Module** (ESP-NOW + UART) |
| **Director** | Physical **ESP32-S3** touchscreen operator desk |
| ~~Stage Engine~~ | **Retired** term |
| Onboard ESP32-C6 (Waveshare P4 module) | **UNUSED BY SHOWDUINO / RESERVED HARDWARE** |
| External SUE / ESP32-C3 SuperMini | **LEGACY / SUPERSEDED** |

---

## Current P4 pin map

Confirmed assignments. Do not change these during architecture-lock work.

| P4 GPIO | Function |
|---------|----------|
| 4 | Comms UART RX (from S3 TX) |
| 5 | Comms UART TX (to S3 RX) |
| 6 | **RESERVED** — onboard C6 control (C6 GPIO2) |
| 7 | Plug-in Bus SDA (Waveshare I²C header / 40-pin pin 3) |
| 8 | Plug-in Bus SCL (Waveshare I²C header / 40-pin pin 5) |
| 14–19 | **RESERVED** — onboard C6 SDIO |
| 20 | PCM5102A WS / LRCK |
| 21 | PCM5102A BCLK |
| 22 | PCM5102A DATA / DOUT |
| 24 | Emergency NeoPixels |
| 25 | Physical emergency button (momentary active-LOW, ~30 ms debounce, software latch) |
| 39–45 | SDMMC Slot 0 (microSD). D0–D3=39–42, CLK=43, CMD=44, POWER=45 |
| 54 | **RESERVED** — onboard C6 reset / CHIP_PU |

Source of truth in firmware: `firmware/stage-engine-p4/ShowduinoStageEngineP4/BoardConfig.h`.

### UART wiring (required)

P4 pins stay GPIO4 RX / GPIO5 TX. The S3 uses different GPIOs.

```text
S3 GPIO17 TX  →  P4 GPIO4 RX
S3 GPIO18 RX  ←  P4 GPIO5 TX
GND shared
115200 8N1, newline-terminated ASCII
```

USB on the S3 is for programming/debug. Do not steal native USB or UART0 for this link.

### Onboard C6 (unused / reserved)

The DEV-KIT onboard C6 and its SDIO/control nets remain reserved hardware. Do not allocate P4 GPIO6, GPIO14–19, or GPIO54. Do not flash the onboard C6 for Showduino.

---

## Internal SDIO (exists, unused)

Internally routed on the Waveshare ESP32-P4-Module. No external SDIO wiring.

| Signal | P4 | C6 |
|--------|----|----|
| CLK | GPIO18 | GPIO19 |
| CMD | GPIO19 | GPIO18 |
| DAT0 | GPIO14 | GPIO20 |
| DAT1 | GPIO15 | GPIO21 |
| DAT2 | GPIO16 | GPIO22 |
| DAT3 | GPIO17 | GPIO23 |
| Reset | GPIO54 | CHIP_PU |
| Extra | GPIO6 | C6 GPIO2 |

**Showduino does not currently use this bus.**  
P4 SDMMC Slot 0 (card on GPIO39–45) and Slot 1 (this C6 bus) can coexist later. Do not move the SD card.

See [`docs/future-p4-c6-sdio-transport.md`](future-p4-c6-sdio-transport.md).

### ESP-Hosted (not current Director transport)

[ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu) uses this SDIO link so the P4 can host C6 Wi-Fi/BLE. Official features are STA/SoftAP/BLE — **not ESP-NOW** ([features.md](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/features.md), [EHM-19](https://github.com/espressif/esp-hosted-mcu/issues/19)). Installing Hosted slave firmware would replace the Showduino C6 ESP-NOW bridge. Do not install it on this generation.

---

## Director (ESP32-S3)

**Firmware:** `firmware/director-esp32-8048s050/`

ESP-NOW only to the dedicated S3 Comms Controller (normal product). Direct Director↔P4 UART is bench/service only and must stay off.

Peer MAC: copy the S3 Comms Controller STA MAC printed at S3 boot into Director `SHOWDUINO_COMMS_MAC_*`.

---

## Dedicated S3 communications controller

**Firmware:** `firmware/s3-comms-controller/`

- Receive Director ESP-NOW desk packets
- Validate Showduino desk packets
- Forward commands to the P4 over UART
- Receive newline-framed P4 responses
- Remember Director MAC; send P4 status back over ESP-NOW

Must not: run the timeline, own show state, host SoftAP/WebUI, initialise BLE, or run ESP-Hosted/custom SDIO.

**FUTURE / RESERVED / NOT IMPLEMENTED:** BLE, Wi-Fi SoftAP/STA, WebUI proxy, OTA.

## Onboard C6 (historical)

**Firmware (not current):** `firmware/p4-c6-espnow-bridge/`

Previous generation used the Waveshare onboard C6 as the Communications Engine. That application path is retired. The physical C6 remains on the module as unused reserved hardware.

---

## Stage Controller / Show Engine (ESP32-P4)

**Firmware:** `firmware/stage-engine-p4/`

Owns production loading, show/timeline/cue runtime, SD (`/showduino/` including `/showduino/webui/`), show and emergency audio, emergency latch, GPIO25, GPIO24 pixels, WebUI origin, safety.

**Local USB maintenance console:** the P4 USB Serial/debug port (115200 8N1, newline-terminated) is an extra input into the same command dispatcher as C6 UART. It does not replace Director → ESP-NOW → C6 → UART. `EMERGENCY:CLEAR` over USB still cannot bypass GPIO25. See [`firmware/stage-engine-p4/README.md`](../firmware/stage-engine-p4/README.md).

**Plug-in Bus:** 3.3V I²C on GPIO7 (SDA) and GPIO8 (SCL). See [`docs/plugin-bus.md`](plugin-bus.md).

---

## Relay nodes

**Firmware:** `firmware/relay-node-esp32/`

ESP-NOW to the S3 Comms Controller, then UART to the P4. Boot relays OFF. Absolute ON/OFF/pulse only.

Future node families (audio, pixel, sensor, motor, R3 terminals) still speak ESP-NOW to the Communications Engine.

---

## Legacy / previous generation

Not the current P4-module stack:

| Hardware | Classification |
|----------|----------------|
| External ESP32-C3 SuperMini (`firmware/c3-supermini-espnow-bridge/`) | LEGACY / SUPERSEDED — previous Communications Engine |
| Onboard ESP32-C6 (`firmware/p4-c6-espnow-bridge/`) | UNUSED / RESERVED HARDWARE |
| CYD 2.8″ (`firmware/controller-cyd/`) | Legacy — archive candidate |
| Arduino Mega executor (`firmware/executor-mega/`) | Legacy — archive candidate |
| SUE ESP32-S3 node stub (`firmware/sue-esp32s3-node/`) | Incomplete historical node family |
| Historical CYD/Mega/SUE pin draft (`docs/hardware-pinout.md`) | Pre–P4-module; not this pin map |

---

## Power

```text
5V  — logic boards, many relay modules, pixels, DFPlayers
12V — props, solenoids, lamps, motors as required
3.3V — ESP32 logic only
```

Shared GND for signal companions. No 5V into ESP32 GPIO. Nodes boot outputs OFF. Emergency policy is P4-local and must work without the S3 Comms Controller or Director.

---

## Minimum demo (this generation)

```text
1x ESP32-S3 5" Director           firmware/director-esp32-8048s050/
1x Waveshare P4-Module-DEV-KIT    P4: firmware/stage-engine-p4/
1x ESP32-S3 Dev Module            firmware/s3-comms-controller/
1x UART pair                      S3 GPIO17→P4 GPIO4, S3 GPIO18←P4 GPIO5
1x ESP32 Relay Node               firmware/relay-node-esp32/
Emergency GPIO25 exercised with S3 unplugged
```

---

## Product family naming

```text
Showduino Director
Showduino Stage Controller   (runs Show Engine)
Showduino Communications Engine  (dedicated ESP32-S3 in this generation)
Showduino Relay Node 4 / 8
Showduino Audio Node
Showduino Pixel Node
Showduino Prop Node
```

Avoid shipping new materials that say “Stage Engine.”

---

## Primary references

Hardware and Hosted conclusions in this document follow vendor sources, not forum posts:

- Waveshare [ESP32-P4-Module-DEV-KIT](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT) and [Resources](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/Resources-And-Documents) (schematic / design files)
- Waveshare [ESP32-P4-Module](https://www.waveshare.com/wiki/ESP32-P4-Module) (module with onboard C6; factory C6 path is SDIO)
- [ESP-Hosted-MCU](https://github.com/espressif/esp-hosted-mcu), [SDIO](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/sdio.md), [features](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/features.md), [P4 function EV setup](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)
- ESP-NOW over Hosted: [espressif/esp-hosted-mcu#19](https://github.com/espressif/esp-hosted-mcu/issues/19)

Future custom SDIO transport (not implemented): [`docs/future-p4-c6-sdio-transport.md`](future-p4-c6-sdio-transport.md).
