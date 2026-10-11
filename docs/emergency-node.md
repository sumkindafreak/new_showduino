# Wireless Emergency + Pixel Node

```text
Status: SOFTWARE IMPLEMENTED / GPIO2+GPIO3 UNVERIFIED / NOT PHYSICALLY VALIDATED
Role: EMERGENCY (+ PIXEL capability on same peer)
Firmware: 0.3.2
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 additive route estop-node-pixels
```

## Purpose

Distributed venue **wireless Emergency Buttons**. Additional stations that may assert the **same** P4 global emergency latch as the P4 main Emergency input.

The same physical ESP32-C3 provides a **full Showduino Pixel Controller** on **GPIO2** and a physically separate **two-pixel local status/atmosphere line on GPIO3**. Logical identity remains `ESTOP-01` … `ESTOP-08`. Do **not** invent a fake `LED-xx` / `PIXEL-01` peer for the pixel line.

This is **not** a certified life-safety system. Wireless ESP-NOW does not replace hardwired emergency stops, fire-alarm, evacuation, machinery-safety, or other regulatory systems an installation requires.

## Architecture

```text
P4 MAIN EMERGENCY BUTTON --------------+
                                      |
ESTOP-01 C3 ---+                      |
ESTOP-02 C3 ---+ ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
ESTOP-03 C3 ---+                      |
     |
     +-- GPIO4 momentary Emergency pushbutton
     +-- GPIO2 WS2812 Show Pixel Line (full FX engine)
     +-- GPIO3 WS2812 status/atmosphere (fixed 2 LEDs)
```

| Piece | Role |
|-------|------|
| P4 GPIO25 main Emergency path | Primary independent Emergency input. Not routed through ESP-NOW. Separate Locate/clear policy. |
| Wireless Emergency + Pixel Node | Momentary pushbutton, local latch, ESP-NOW assert, SSD1306 OLED, full Pixel engine on GPIO2, and independent fixed two-LED status/atmosphere on GPIO3 |
| Comms | Transport / discovery / priority UART forward |
| P4 | Global emergency authority; strips `ESTOP:NODE:PIXEL:` → `PIXEL:` to same peer |
| Director | Operator status and source display; PIXEL capability badge if present |
| Studio | Inventory + `estop-node-pixels` route (showduino.com follow-up if needed) |

## Absolute rule

**ASSERT ONLY — NEVER CLEAR.**

No Emergency Node packet, WebUI, button-release, or reboot may clear P4 emergency, hardwired emergency, another station, or the global latch.

Clear remains the existing Showduino emergency-clear policy.

`ESTOP:ACK:LATCHED` means “P4 received and latched your assert.” It does **not** mean emergency cleared.

## Identity

- Logical ID (authoritative): `ESTOP-01` … `ESTOP-08`
- Friendly name (presentation): Entrance, Control Room, Maze Exit, …
- Capabilities: `EMERGENCY` + `PIXEL` (same peer)
- Routing uses logical ID, never MAC as operator identity
- System limit: **8** stations

## Momentary pushbutton

Physical model: normally-open momentary button to GND, `INPUT_PULLUP` on **GPIO4**.

- RELEASED → HIGH
- PRESSED → LOW → local latch + `ESTOP:ASSERT` **first**, then Pixel Emergency White

GPIO4 remains **physically unverified** until bench commissioning.

## Local latch

PRESS latches locally and asserts. RELEASE never unlatches and never sends a clear.

After a legitimate global clear is **observed**:

- button RELEASED → local station returns to NORMAL / READY; Pixel line → **BLACK** (no auto-resume of prior FX)
- button still PRESSED → station remains latched and re-asserts; pixels remain Emergency White

Optional GPIO9 long-press is **maintenance-only** local reset. It never clears P4.

## Show Pixel Line (GPIO2)

Full Showduino Pixel engine (shared with `firmware/c3-pixel-node/` via `firmware/shared-pixel/`):

- COUNT / INIT / persistence
- Segments + all **25** FX
- Locate, commissioning TEST, brightness, colours, speed, intensity, randomness, duration
- Command prefix: `ESTOP:NODE:PIXEL:<pixel command>` → on-wire `PIXEL:<pixel command>`
- Max **512** pixels target — validate Emergency loop timing on hardware before signing off long lines
- Recommend **330 Ω** series data resistor

Emergency White overrides all attraction FX, TEST, and Locate.

## Independent local status / atmosphere (GPIO3)

Exactly **two physical NeoPixels** daisy-chained to GPIO3, using a different data output and buffer from the theatrical GPIO2 line. No operator `PIXEL:` cue can overwrite GPIO3 status. GPIO2 remains fully configurable.

Priority: **Emergency (both white)** → station fault (magenta/amber alternating) → Locate (alternating white) → boot (violet) → radio searching/lost (amber chase) → ready (turquoise status + violet breathing atmosphere).

The status strip is serviced after emergency GPIO sampling and assert TX; it never sends `ESTOP:CLEAR`. Hardware mapping is not yet verified; GPIO3, GPIO4 (button), GPIO2 (show pixels), GPIO5/6 (OLED), and GPIO9 (maintenance reset) must all be checked on the actual board. `BoardConfig.h` enforces distinct pin assignments at compile time.

Wire GPIO3 → 330 Ω → DIN of pixel 0, DOUT of pixel 0 → DIN of pixel 1; supply +5V and common GND. Use an appropriate level shifter if required, never put 5V onto ESP32 GPIO, and allow up to about 120mA worst-case for 2 WS2812 RGB LEDs at full white. Status is visible in node WebUI and serial diagnostics.

**Release/OTA constraint:** This is a GitHub source update, **not** an OTA installation path for Emergency Nodes. Emergency Nodes remain USB-flashed and must be updated/validated one at a time; Comms-only self-OTA is documented in [system-updates.md](system-updates.md).

## OLED

Primarily Emergency status. NORMAL may show `PIX <count>`. During Emergency: `PIX WHITE`.

## SoftAP

Existing Emergency SoftAP with a compact PIXELS section (not a full Pixel Node WebUI clone).

## Related

- [`c3-pixel-node.md`](c3-pixel-node.md) — standalone Pixel Node (still supported)
- [`command-protocol.md`](command-protocol.md)
- Firmware: `firmware/c3-emergency-node/README.md`
