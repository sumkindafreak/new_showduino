# Showduino Director — ESP32-8048S043 / 8048S050

```text
Status: ACTIVE
Role: Showduino Director
```

Commands and displays only. Canonical active Director firmware.

```text
This touchscreen Director ESP32-S3   (firmware/director-esp32-8048s050/)
    → ESP-NOW (wireless only; no P4 UART)
Standalone ESP32-S3 Comms Controller (firmware/s3-comms-controller/)
    → UART 115200
Show Engine ESP32-P4                 (firmware/stage-engine-p4/)
```

This board is the operator desk. It is **not** the Comms Controller. The Comms Controller is a separate ESP32-S3 Dev Module with no touchscreen.

This sketch does **not** host or proxy the primary Web UI. USB Serial is for flash/diagnostics only, not the normal show path.

## Architectural notes

- The Show Engine is the single source of truth. Director actions are **requests**.
- Existing SD show/config helpers under `ShowduinoDirector8048S050/src/` are **temporary implementation details**, not the final authoritative project store.
- Application policy: absolute relay states (ON/OFF), not distributed `TOGGLE` (firmware may still contain legacy TOGGLE — to be removed in a later stage).
- Display of successful output state should follow Show Engine confirmation (ACK / state publish); that behaviour is a known follow-up, not claimed complete here.

## What this firmware includes today

- 800×480 ST7262 RGB + GT911 touch + LVGL 9 UI
- ESP‑NOW transport to the Communications Engine (`EspNowTransport.h`)
- Optional UART fallback flags in `BoardConfig.h` (keep off for normal use)
- Emergency / live control / diagnostics screens
- SD storage subsystem for UI assets and temporary data

## Sketch location

```text
firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
```

Diagnostic sibling (not product firmware):

```text
firmware/director-esp32-8048s050/ShowduinoSdTouchTest/
```

## Pairing (current implementation)

1. Flash and run `firmware/s3-comms-controller/` on a **separate** ESP32-S3 Dev Module (not this touchscreen).
2. Note that board's Wi-Fi MAC from USB Serial at boot (`[COMMS] Wi-Fi MAC: …`).
3. Set peer MAC in `ShowduinoDirector8048S050/BoardConfig.h` (`SHOWDUINO_COMMS_MAC_*` — ESP-NOW destination of that standalone Comms Controller).
4. Flash this Director.
5. Confirm link READY via HELLO / HEARTBEAT.

Logical device IDs (not raw MACs) are the long-term application addressing model; MAC fields remain a transport-layer concern until the ID map lands on the Show Engine / Communications Engine.

## Arduino IDE (Director)

- Board: ESP32S3 Dev Module  
- **USB CDC On Boot: Disabled** (required)  
- USB Mode: USB-OTG (TinyUSB)  
- Flash: 16MB, QIO 80MHz  
- **PSRAM: OPI PSRAM** (required)  
- Serial Monitor: **115200** on the CH340 COM port (the one that prints `ESP-ROM:esp32s3-…`)

This panel’s USB-C serial chip is CH340 on UART0 (GPIO43/44). Native USB CDC uses GPIO19/20, which are the GT911 I2C pins. If CDC is left Enabled, the bootloader still prints on the CH340 port and then the sketch goes silent.

Libraries: `lvgl` 9.x, `Arduino_GFX_Library`, `TAMC_GT911`, `Adafruit NeoPixel` (ambient LEDs on GPIO17).

## Related active stack

| Role | Path |
|------|------|
| Communications Engine | `firmware/s3-comms-controller/` |
| Show Engine (Stage Controller) | `firmware/stage-engine-p4/` |
| Relay Node | `firmware/relay-node-esp32/` |

See `docs/architecture.md` and root `README.md`. Classification: [`docs/repository-status.md`](../../docs/repository-status.md).
