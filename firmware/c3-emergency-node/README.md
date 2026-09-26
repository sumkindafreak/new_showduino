# Showduino C3 Emergency + Pixel Node

```text
Status: IMPLEMENTED / GPIO UNCONFIRMED / PIXEL CAPABILITY ADDED
Role: Specialist wireless emergency station (ASSERT ONLY) + full Show Pixel Line
Hardware: ESP32-C3 Super Mini OLED
Firmware: 0.3.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

This is an **additional** distributed wireless Emergency Button station. It does **not** replace the P4 GPIO25 hardwired main Emergency path.

The same physical peer (`ESTOP-01` … `ESTOP-08`) also provides the **full Showduino Pixel Controller** on **GPIO2**. Pixel is a **capability** of the Emergency Node — not a fake `LED-xx` identity.

```text
P4 MAIN EMERGENCY BUTTON ------------+
                                    |
ESTOP-xx C3 --- ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
   | GPIO4 momentary Emergency pushbutton
   | GPIO2 WS2812 Show Pixel Line (full FX engine)
```

## Absolute rule

An Emergency Node may **ASSERT**.

An Emergency Node must **never CLEAR**.

There is no `ESTOP:CLEAR`. Button release does not clear. WebUI does not clear. Reboot does not clear P4 emergency.

Emergency handling always has priority over theatrical Pixel processing.

## Sketch

```text
firmware/c3-emergency-node/ShowduinoC3EmergencyNode/
```

Shared Pixel engine:

```text
firmware/shared-pixel/
```

Arduino FQBN:

```text
esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio
```

Do **not** mark GPIO verified until the momentary pushbutton on GPIO4 is physically confirmed.

## GPIO

| Pin | Function |
|-----|----------|
| GPIO2 | Show Pixel data (WS2812 / NeoPixel, GRB 800 kHz). Recommend **330 Ω** series resistor. |
| GPIO4 | Momentary Emergency pushbutton (`INPUT_PULLUP`, pressed LOW) |
| GPIO5 | OLED SDA |
| GPIO6 | OLED SCL |
| GPIO9 | Optional maintenance local reset (long-press). Never clears P4. |

## Pixel capability

- Same command model as `firmware/c3-pixel-node/`
- Up to **512** pixels (validate loop timing / Emergency responsiveness on hardware)
- All **25** Showduino FX, segments, COUNT/INIT, persistence, Locate, commissioning TEST
- Studio / SHDO route: `estop-node-pixels` → `ESTOP:NODE:PIXEL:<cmd>`
- P4 strips prefix and forwards `PIXEL:<cmd>` to the same ESTOP ESP-NOW peer

When Emergency is latched: entire configured line → **Emergency White**. After legitimate clear with button released: line goes **BLACK** and does **not** auto-resume prior FX.

## SoftAP

SSID pattern: `Showduino-EStop-<id>` — password `showduino`. Compact PIXELS page for COUNT/INIT/TEST/segments.
