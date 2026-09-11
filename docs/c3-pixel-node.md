# Showduino C3 Pixel Node

```text
Status: IMPLEMENTED / NEEDS HARDWARE TEST
Role: Remote Show Pixel Line (same model as P4 GPIO23)
Hardware: ESP32-C3 Super Mini OLED
Firmware: 0.1.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

The Pixel Node is **not** a second lighting console.

> The C3 Pixel Node is a remote equivalent of the P4 GPIO23 Show Pixel Line.

```text
PIXEL LINE → SEGMENTS → EFFECTS → PARAMETERS
```

P4 remains authoritative. Comms transports intent. The node renders locally. GPIO24 emergency signage is unrelated and must stay separate.

Electrical pinout, commissioning, emergency, Locate, and communication-loss policy live in [`firmware/c3-pixel-node/README.md`](../firmware/c3-pixel-node/README.md).

## Integration

| Surface | Behaviour |
|---------|-----------|
| C3 firmware | One WS2812 line on **GPIO2**. OLED SDA **GPIO5** / SCL **GPIO6**. Count 1–512. Save Count does not light. Initialise Line starts the driver. |
| P4 | `PixelNodeLink` — up to 8 nodes. `PIXEL:NODE:<id>:…`. Offline target → `REJECTED:PIXEL:OFFLINE:<id>` and timeline continues. |
| Comms | Multi-slot ESP-NOW peers. `ROUTE:PIXEL:<id>:<seq>:<cmd>`. Follow-channel. Never deinit ESP-NOW / delete Director/Audio/Lamp peers. |
| Director | Page 04 **PIXEL NODE** card. Footer neopixel slot. Detail wire `STATE:NODE:PIXEL:D:`. |
| Studio commissioning | Outputs page: P4 GPIO23 **and** per-node cards with the same Save Count → Initialise Line model. |
| SHDO v2 | Device `binding.route = "pixel-node"` + `nodeId`. Compiles `PIXEL:NODE:<id>:SEGMENT:…`. Not SHDO v3. |

## Communication loss

If P4 GRANT / keepalive is lost: **blackout**. Stale FX are not resumed. OLED reports loss. Other nodes and the P4 timeline continue.

## Hardware tests

The first-physical, multi-node, and radio-torture checklists in [`docs/physical-test-checklist.md`](physical-test-checklist.md) remain **NEEDS HARDWARE TEST**. Compilation is not a hardware pass.
