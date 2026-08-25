# Showduino Director — ESP32-8048S043 / 8048S050

```text
Status: ACTIVE
Role: Showduino Director
```

Commands and displays only. Canonical active Director firmware.

## Final target path

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine — onboard ESP32-C6 in the Stage Controller
    → integrated P4/C6 transport
Show Engine — ESP32-P4
```

The Director does not host authoritative show state. USB Serial remains flash/diagnostics only.

## Architectural notes

- The Show Engine is the single source of truth. Director actions are requests.
- Successful output/show state must follow authoritative Show Engine confirmation.
- The Director never directly controls Stage Controller/node GPIOs.
- Director-local SD/UI assets are implementation details, not the final authoritative show store.

## What this firmware includes

- 800×480 RGB display + GT911 touch + LVGL 9 UI
- ESP-NOW transport through `EspNowTransport.h`
- Emergency / live control / diagnostics screens
- SD storage for Director UI assets/local data
- Optional diagnostic/legacy UART features kept off for normal operation

## Communications migration

The Director previously paired to the separate external C3 SuperMini Communications Engine. The final hardware target is now the **onboard ESP32-C6** in the Waveshare P4 Stage Controller.

The existing peer macros named `SHOWDUINO_P4_C6_MAC_*` are historical but now happen to match the intended hardware role again: once the onboard C6 is brought up, they should contain the **onboard C6 ESP-NOW peer MAC**.

Do not guess that MAC. Read it from the actual C6 firmware/board during bring-up.

### Target pairing flow

1. Bring up the Stage Controller's onboard C6 Communications Engine.
2. Read/record its ESP-NOW station MAC.
3. Set the Director peer MAC in `ShowduinoDirector8048S050/BoardConfig.h`.
4. Flash the Director.
5. Confirm Director → C6 → P4 requests.
6. Confirm P4 → C6 → Director ACK/state replies.
7. Confirm emergency traffic and link recovery.

### Compatibility pairing

The old external C3 path remains in `firmware/c3-supermini-espnow-bridge/` only as rollback/reference firmware during migration.

## Sketch location

```text
firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
```

Diagnostic sibling:

```text
firmware/director-esp32-8048s050/ShowduinoSdTouchTest/
```

## Arduino IDE

- Board: ESP32S3 Dev Module
- USB CDC On Boot: Enabled
- Flash: 16MB
- PSRAM: OPI PSRAM
- Serial: 115200

Libraries include LVGL 9.x, Arduino_GFX_Library, TAMC_GT911 and Adafruit NeoPixel where used by the current UI hardware.

## Related stack

| Role | Path |
|------|------|
| Communications Engine target | `firmware/p4-c6-espnow-bridge/` |
| Communications compatibility | `firmware/c3-supermini-espnow-bridge/` |
| Show Engine / Stage Controller | `firmware/stage-engine-p4/` |
| Relay Node | `firmware/relay-node-esp32/` |

See `docs/architecture.md`, `docs/hardware-baseline-2026-08-25.md`, and root `README.md`.
