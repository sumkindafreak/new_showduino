# Showduino Director — ESP32-8048S043 / 8048S050

```text
Status: ACTIVE
Role: Showduino Director
```

Commands and displays only. Canonical active Director firmware.

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

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

1. Flash and run `firmware/c3-supermini-espnow-bridge/` (Communications Engine).
2. Note the C3 MAC from Serial.
3. Set peer MAC in `ShowduinoDirector8048S050/BoardConfig.h` (`SHOWDUINO_P4_C6_MAC_*` names are historical; values must be the **C3** Communications Engine MAC).
4. Flash this Director.
5. Confirm link READY via HELLO / HEARTBEAT.

Logical device IDs (not raw MACs) are the long-term application addressing model; MAC fields remain a transport-layer concern until the ID map lands on the Show Engine / Communications Engine.

## Arduino IDE (Director)

- Board: ESP32S3 Dev Module  
- USB CDC On Boot: Enabled  
- Flash: 16MB, QIO 80MHz  
- **PSRAM: OPI PSRAM** (required)  
- Serial: 115200  

Libraries: `lvgl` 9.x, `Arduino_GFX_Library`, `TAMC_GT911`.

## Related active stack

| Role | Path |
|------|------|
| Communications Engine | `firmware/c3-supermini-espnow-bridge/` |
| Show Engine (Stage Controller) | `firmware/stage-engine-p4/` |
| Relay Node | `firmware/relay-node-esp32/` |

See `docs/architecture.md` and root `README.md`. Classification: [`docs/repository-status.md`](../../docs/repository-status.md).
