# Showduino C3 Lamp Node

```text
Status: ACTIVE
Role: Specialist lamp / FX node
Hardware: ESP32-C3 Super Mini OLED (HUNT-proven pin map)
Firmware: 0.2.0
```

This is a **specialist output node**, not a communications engine.

It is **not** the superseded ESP32-C3/SUE Comms path (`firmware/c3-supermini-espnow-bridge/`).
It is **not** the unused/reserved onboard ESP32-C6.

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
ESP32-S3 Communications Engine
        │ UART
        ▼
ESP32-P4 Show Engine
        │ NODE:LAMP: / LAMP:*
        ▼
this C3 Lamp Node  (ESP-NOW)
```

The P4 remains authoritative. The node executes local carbide-style lamp FX and reports what happened.

## Sketch

```text
firmware/c3-lamp-node/ShowduinoC3LampNode/
```

Arduino profile used in HUNT: MakerGO ESP32 C3 SuperMini, otherwise ESP32C3 Dev Module.

- USB CDC On Boot: enabled
- Flash: DIO, 4 MB
- No PSRAM

```text
arduino-cli compile --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio" firmware/c3-lamp-node/ShowduinoC3LampNode
```

## Pins (do not guess)

Copied from the HUNT player-node map. Do not reassign without hardware confirmation.

| GPIO | Function |
|------|----------|
| 5 / 6 | OLED SDA / SCL (SSD1306 0x3C) |
| 9 | Button A (BOOT / local test) |
| 0 | Button B (OLED page / STATUS) |
| 2 | Lamp NeoPixels (7 × WS2812, original carbide lamp data pin) |

Unused HUNT GPIOs (3, 4, 7, 8, 10) are left alone.

## Ownership

Same GRANT / standalone model as the Audio Node. See [`docs/standalone-node-architecture.md`](../../docs/standalone-node-architecture.md).

Emergency on the node forces lamp pixels bright white. Clearing emergency returns to safe idle; it does not resume the previous FX.

## Protocol

See `protocol/showduino_lamp_node.h`. P4 link: `firmware/stage-engine-p4/.../src/nodes/LampNodeLink.*`.
