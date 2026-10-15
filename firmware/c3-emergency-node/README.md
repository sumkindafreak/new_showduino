# Showduino C3 Emergency + Pixel Node

```text
Status: CODE IMPLEMENTED / GPIO2 + GPIO3 UNVERIFIED / HARDWARE TEST REQUIRED
Role: Specialist wireless emergency station (ASSERT ONLY) + full Show Pixel Line
Hardware: ESP32-C3 Super Mini OLED
Firmware: 0.3.2
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
```

This is an **additional** distributed wireless Emergency Button station. It does **not** replace the P4 GPIO25 hardwired main Emergency path.

The same physical peer (`ESTOP-01` … `ESTOP-08`) provides the **full Showduino Pixel Controller** on **GPIO2**, plus a physically separate **fixed two-LED local status/atmosphere line on GPIO3**. Pixel is a **capability** of the Emergency Node — not a fake `LED-xx` identity.

```text
P4 MAIN EMERGENCY BUTTON ------------+
                                    |
ESTOP-xx C3 --- ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
   | GPIO4 momentary Emergency pushbutton
   | GPIO2 WS2812 Show Pixel Line (full FX engine)
   | GPIO3 WS2812 two-pixel local status / atmosphere
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
| GPIO2 | Full Show Pixel data (WS2812 / NeoPixel, GRB 800 kHz). Recommend **330 Ω** series resistor. |
| GPIO3 | Dedicated 2-LED local status/atmosphere NeoPixel line, fixed count and separate buffer. Recommend **330 Ω** series data resistor. **Proposed pin: bench verification required.** |
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

## Dedicated local status / atmosphere (GPIO3)

This is a **separate physical strip of exactly 2 WS2812/NeoPixels**, not the first two LEDs of GPIO2. It is **not** controlled by `PIXEL:COUNT`, Show FX, segments or the Director. The GPIO2 count/persistence remains untouched.

| Priority | Two-pixel behaviour |
|---|---|
| Local latch, held emergency input, or system emergency active | Both **white**, overriding everything |
| Station fault / needs rearm | Alternating magenta / amber |
| Locate (existing GPIO2 locate state) | White alternating pixel chase |
| Boot | Dim violet pair |
| Radio lost/searching | Alternating dim amber |
| Normal/linked | Status pixel steady turquoise; atmospheric pixel slowly breathes violet |

All animation is non-blocking and runs **after Emergency input and assertion processing**. Brightness, data pin and frame rate are set in `BoardConfig.h` (the fixed count is intentionally 2). WebUI `/api/status` reports `statusPixelGpio`, `statusPixelCount`, `statusPixelReady`, and `statusPixelMode`. Serial `STATUS` / `PINS` report both distinct lines.

**Wiring:** GPIO3 → 330 Ω resistor → first pixel DIN; first DOUT → second DIN. Connect both pixels to a suitable 5V supply and **common ground with the C3**. Use a 3.3V-to-5V logic-level shifter if the data signal is unreliable; never connect 5V to GPIO3. Check GPIO3 availability on the specific Super Mini OLED PCB first, and verify that adding pixels does not change emergency input responsiveness. Two fully white WS2812 pixels can draw up to about **120 mA** worst-case, so budget the supply accordingly.

**This commit is not a node OTA installer.** Emergency Node flashing remains via USB; only the Comms Controller currently has self-OTA. Update Emergency stations **one at a time** with the hardwired P4 emergency available; check the emergency button and global latch after installation.

## SoftAP

SSID pattern: `Showduino-EStop-<id>` — password `showduino`. Compact PIXELS page for COUNT/INIT/TEST/segments.
