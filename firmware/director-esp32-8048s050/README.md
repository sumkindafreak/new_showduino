# Showduino Director — ESP32-8048S043 / 8048S050

```text
Status: ACTIVE
Role: Showduino Director
```

Commands and displays only. Canonical active Director firmware.

```text
Director ESP32-S3 touchscreen
    → ESP-NOW
Dedicated ESP32-S3 Communications Engine
    → UART 115200 8N1
P4 Show Engine / Stage Controller
```

The Director is the operator desk. It is **not** the Communications Engine and it does not host the canonical browser Studio.

## Current browser/WebUI split

```text
Browser / phone / tablet
    → Wi-Fi SoftAP on dedicated S3 Communications Engine
    → Showduino Studio static frontend from S3 PROGMEM
    → S3 API proxy over UART
    → P4 authoritative Web API/state
```

The Director does not host/proxy this WebUI. USB Serial is for flash/diagnostics only, not the normal show path.

## Architectural rules

- The P4 Show Engine is the single source of truth.
- Director actions are **requests**.
- A running show must not depend on the Director being powered/connected.
- Existing Director SD helpers are local/temporary implementation details, not the authoritative production store.
- Successful remote-output display should follow authoritative P4/Node state rather than optimistic transport success.
- Application addressing is moving toward logical Showduino device IDs; MAC addresses are transport details.
- Emergency policy is owned by the P4.

## Current pixel/emergency policy relevant to the Director

The Director may display/request pixel state but does not execute FX.

P4 local pixel roles:

```text
GPIO23  Main Show Pixel Line — segmented FX engine
GPIO24  emergency/signage line — 10-pixel sign groups
```

Hard Showduino rule:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

That applies to P4 local lines and all future pixel-capable Nodes. The Director must not expose a control that disables/recolours this safety override. Clearing emergency does not automatically resume the show or old pixel effects.

## What this firmware includes today

- 800×480 ST7262 RGB + GT911 touch + LVGL 9 UI;
- ESP-NOW transport to the dedicated Communications Engine;
- optional UART fallback/service flags kept off for normal use;
- emergency/live/diagnostic UI;
- Nodes and Audio Node pages/controls;
- SD storage subsystem for Director-local UI assets/temporary data.

The Director is not the current Studio pixel-authoring surface; segment commissioning/authoring belongs in the S3-hosted browser Studio, with P4 as execution authority.

## Sketch location

```text
firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
```

Diagnostic sibling:

```text
firmware/director-esp32-8048s050/ShowduinoSdTouchTest/
```

## Pairing

1. Flash/run `firmware/s3-comms-controller/` on a separate ESP32-S3 Dev Module.
2. Note that S3 board's Wi-Fi/ESP-NOW MAC from USB Serial at boot.
3. Set the Director peer in `ShowduinoDirector8048S050/BoardConfig.h` using `SHOWDUINO_COMMS_MAC_*`.
4. Flash the Director.
5. Confirm HELLO/HEARTBEAT/link state.

Logical device IDs are the application-level target. MAC values remain transport-layer configuration until end-to-end ID resolution is complete.

## Arduino IDE

- Board: ESP32S3 Dev Module
- **USB CDC On Boot: Disabled**
- USB Mode: USB-OTG (TinyUSB)
- Flash: 16MB, QIO 80MHz
- **PSRAM: OPI PSRAM**
- Serial Monitor: **115200** on the CH340 UART port

This panel’s USB-C serial chip is CH340 on UART0 (GPIO43/44). Native USB CDC uses GPIO19/20, which overlap the GT911 I²C pins, so CDC-on-boot must remain disabled for this panel baseline.

Libraries include LVGL 9.x, Arduino_GFX_Library, TAMC_GT911 and Adafruit NeoPixel for Director-local ambient LEDs.

## Active stack / roadmap

| Role | Path / status |
|------|---------------|
| Communications Engine | `firmware/s3-comms-controller/` — ACTIVE, including S3-hosted Studio |
| Show Engine | `firmware/stage-engine-p4/` — ACTIVE |
| Audio Node | `firmware/audio-node-esp32-a1s/` — implemented firmware / hardware test required |
| C3 Lantern Node | next specialist Node, work only when explicitly started |
| C3 Pixel Node | follows Lantern |
| MOSFET Node | follows C3 Pixel; replaces old Relay Node direction |
| Relay Node | legacy/superseded reference only |
| DMX | parked/out of scope |

See `docs/architecture.md`, `docs/studio-pixel-authoring.md` and root `README.md`. Classification: [`docs/repository-status.md`](../../docs/repository-status.md).
