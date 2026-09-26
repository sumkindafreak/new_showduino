# Wireless Emergency Node

```text
Status: SOFTWARE IMPLEMENTED / GPIO UNCONFIRMED / OLED ADDED / NOT PHYSICALLY VALIDATED
Role: EMERGENCY
Firmware: 0.2.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 unchanged
```

## Purpose

Distributed venue **wireless Emergency Buttons**. Additional stations that may assert the **same** P4 global emergency latch as the P4 main Emergency input.

This is **not** a certified life-safety system. Wireless ESP-NOW does not replace hardwired emergency stops, fire-alarm, evacuation, machinery-safety, or other regulatory systems an installation requires.

## Architecture

```text
P4 MAIN EMERGENCY BUTTON --------------+
                                      |
ESTOP-01 C3 ---+                      |
ESTOP-02 C3 ---+ ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
ESTOP-03 C3 ---+                      |
```

| Piece | Role |
|-------|------|
| P4 GPIO25 main Emergency path | Primary independent Emergency input. Not routed through ESP-NOW. Separate Locate/clear policy. |
| Wireless Emergency Node | Momentary pushbutton, local latch, ESP-NOW assert, SSD1306 status OLED |
| Comms | Transport / discovery / priority UART forward |
| P4 | Global emergency authority |
| Director | Operator status and source display |
| Studio | Inventory only. Not a timeline cue. |

## Absolute rule

**ASSERT ONLY — NEVER CLEAR.**

No Emergency Node packet, WebUI, button-release, or reboot may clear:

- P4 emergency
- hardwired / main-button emergency
- another station's emergency
- the global latch

Clear remains the existing Showduino emergency-clear policy.

`ESTOP:ACK:LATCHED` means “P4 received and latched your assert.” It does **not** mean emergency cleared.

## Identity

- Logical ID (authoritative): `ESTOP-01` … `ESTOP-08`
- Friendly name (presentation): Entrance, Control Room, Maze Exit, …
- Routing uses logical ID, never MAC as operator identity
- System limit: **8** stations (same ESP-NOW multi-peer cap as Pixel)

## Momentary pushbutton

Physical model: normally-open momentary button to GND, `INPUT_PULLUP` on **GPIO4**.

- RELEASED → HIGH
- PRESSED → LOW → local latch + `ESTOP:ASSERT`

GPIO4 remains **physically unverified** until bench commissioning. Do not set the verified flag until confirmed.

## Local latch

PRESS latches locally and asserts. RELEASE never unlatches and never sends a clear.

After a legitimate global clear is **observed**:

- button RELEASED → local station returns to NORMAL / READY automatically
- button still PRESSED → station remains latched and re-asserts

Optional GPIO9 long-press is **maintenance-only** local reset. It never clears P4 and is not required for normal operator use.

## OLED

Same proven C3 Pixel Node SSD1306 hardware:

- SDA GPIO5 / SCL GPIO6 / 0x3C / 128×64 / 400 kHz / 180° / Pixel viewport crop

OLED is output-only. Init failure must never block button detect, latch, or radio assert.

## Offline policy (V1)

An Emergency Node going offline is a **SAFETY NODE FAULT / warning**.

It does **not** automatically assert global emergency.

Reason: temporary radio interference must not repeatedly kill a running attraction.

The fault must be prominent on P4 / Director / operator surfaces and must not look like the global emergency screen.

## Sequential update / commissioning

Never flash or reboot two Emergency Nodes at the same time.

```text
ESTOP-01 update
        ↓
      reboot
        ↓
 healthy + linked
        ↓
ESTOP-02 update
        ↓
 healthy + linked
        ↓
ESTOP-03 ...
```

`HEALTHY` = booted, NC input readable, no local fault.  
`LINKED` = Comms/P4 have a fresh announce/heartbeat for that logical ID.

Host helper: `showduino_emergency_update_gate()` in `protocol/showduino_emergency_node.h`.

## Radio

Follow the current Showduino ESP-NOW channel. Do not hardcode channel 1. Do not deinit ESP-NOW to follow a venue AP.

If radio is down while the button is pressed, the local latch is retained. When radio returns, the node immediately asserts the still-latched emergency.

If Comms or P4 reboot while a station is still latched, the node keeps advertising assert and P4 re-enters/retains emergency.

## Trust model

Validated 116-byte `ShowduinoNodePacket`, `nodeType=EMERGENCY`, registered peer. No expensive authentication handshake that would delay assertion. Malformed packets are rejected. This is venue-trust, not a certified secure safety bus.

## Show files

Emergency Node operation does **not** depend on SHDO, the current production, timeline, cue, Studio, or a loaded show.

Emergency is **not** a theatrical SHDO action. Do not drag “EMERGENCY STOP” onto a timeline.

## Physical acceptance matrix

| ID | Test |
|----|------|
| A | Hardwired P4 emergency only |
| B | ESTOP-01 emergency |
| C | ESTOP-02 emergency |
| D | Two wireless E-stops simultaneously |
| E | Wireless E-stop while show running |
| F | Wireless E-stop while no show loaded |
| G | Wireless E-stop while Lamp burning |
| H | Wireless E-stop while Audio playing |
| I | Wireless E-stop while Pixel effects running |
| J | Emergency Node radio loss |
| K | Radio recovery with local latch active |
| L | Comms reboot while wireless emergency latched |
| M | P4 reboot while wireless emergency latched |
| N | Emergency Node offline warning (not global emergency) |
| O | Venue AP / ESP-NOW non-channel-1 operation |

Compilation and host tests do **not** physically validate this safety feature.
