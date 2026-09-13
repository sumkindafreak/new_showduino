# Wireless Emergency Node

```text
Status: SOFTWARE IMPLEMENTED / GPIO UNCONFIRMED / NOT PHYSICALLY VALIDATED
Role: EMERGENCY
Firmware: 0.1.0
Product: Showduino 1.0.0-rc.1
Protocol: 1.0
SHDO: v2 unchanged
```

## Purpose

Distributed venue emergency buttons. Additional wireless stations that may assert the **same** P4 global emergency latch as the hardwired input.

This is **not** a certified life-safety system. Wireless ESP-NOW does not replace hardwired E-stop, fire-alarm, evacuation, machinery-safety, or other regulatory systems an installation requires.

## Architecture

```text
HARDWIRED NC E-STOP -------------------+
                                      |
ESTOP-01 C3 ---+                      |
ESTOP-02 C3 ---+ ESP-NOW -> COMMS -> P4 +-> GLOBAL EMERGENCY LATCH
ESTOP-03 C3 ---+                      |
```

| Piece | Role |
|-------|------|
| P4 GPIO25 hardwired path | Primary independent emergency input. Not routed through ESP-NOW. |
| Emergency Node | Local NC input, local latch, ESP-NOW assert |
| Comms | Transport / discovery / priority UART forward |
| P4 | Global emergency authority |
| Director | Operator status and source display |
| Studio | Inventory only. Not a timeline cue. |

## Absolute rule

**ASSERT ONLY — NEVER CLEAR.**

No Emergency Node packet, WebUI, button-release, or reboot may clear:

- P4 emergency
- hardwired emergency
- another station's emergency
- the global latch

Clear remains the existing Showduino emergency-clear policy.

`ESTOP:ACK:LATCHED` means “P4 received and latched your assert.” It does **not** mean emergency cleared.

## Identity

- Logical ID (authoritative): `ESTOP-01` … `ESTOP-08`
- Friendly name (presentation): Entrance, Control Room, Maze Exit, …
- Routing uses logical ID, never MAC as operator identity
- System limit: **8** stations (same ESP-NOW multi-peer cap as Pixel)

## NC input

Preferred: mushroom NC contact to GND, `INPUT_PULLUP`.

- CLOSED / healthy → LOW
- OPEN (pressed or local wire broken) → HIGH → emergency condition

V1 does **not** distinguish button press from a broken local conductor. Opening the loop is emergency.

GPIO map is **unconfirmed**. Do not flash the C3 until the bench wiring is commissioned.

## Local latch

OPEN latches locally. Closing the mushroom does not unlatch and does not send a clear.

After a legitimate global clear, the station enters `NEEDS_REARM`. Local re-arm (WebUI / optional GPIO9) only resets **this** station. It never clears P4 emergency. If the NC loop is still open, re-arm fails and the station re-asserts.

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
