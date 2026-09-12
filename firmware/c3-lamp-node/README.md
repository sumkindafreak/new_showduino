# Showduino C3 Lamp Node — HISTORICAL

```text
Status: SUPERSEDED / retained as reference
Role: Historical lamp / FX node
Hardware: ESP32-C3 Super Mini OLED (HUNT-proven pin map)
Firmware: 0.2.1
```

**The physical production Lamp Node is an ESP32-S3 carbide-lamp simulator.**

Do not treat this C3 tree as the live target. The current firmware lives in [`firmware/s3-lamp-node/`](../s3-lamp-node/README.md). This folder is kept because useful architecture-independent lamp code and the original carbide FX set still inform the S3 rebuild.

```text
Previous assumption: ESP32-C3 Super Mini OLED
Physical audit:     ESP32-S3 Dev Module family (same as Comms)
```

C3-specific assumptions that must not be copied forward:

- FQBN `esp32c3` / Super Mini OLED / HUNT crop
- OLED required (fault if OLED fails)
- Jewel DATA on GPIO2 (HUNT-unused C3 pin — **not** the S3 lamp wiring)
- Buttons on GPIO9 / GPIO0
- Capability string advertised `OLED`
- No microphone, light sensor, voltage sensor, or Fermion UART

## Sketch (reference only)

```text
firmware/c3-lamp-node/ShowduinoC3LampNode/
```

```text
arduino-cli compile --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio" firmware/c3-lamp-node/ShowduinoC3LampNode
```

## Pins (C3 HUNT board only — not the physical S3 lamp)

| GPIO | Function |
|------|----------|
| 5 / 6 | OLED SDA / SCL (SSD1306 0x3C) |
| 9 | Button A (BOOT / local test) |
| 0 | Button B (OLED page / STATUS) |
| 2 | Lamp NeoPixels (7 × WS2812 on the C3 HUNT board) |

## Ownership / emergency

Same GRANT / standalone model as other specialist nodes.

Emergency on this historical firmware forces lamp pixels bright white. Clearing emergency returns to safe idle; it does not resume the previous FX. The S3 carbide node currently keeps that product policy and documents the semantic conflict with a simulated flame.

Protocol: `protocol/showduino_lamp_node.h`. Production target: `firmware/s3-lamp-node/`.
