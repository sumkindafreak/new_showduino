# Simplified ESP32-C3 Lamp Node: integration status and specification

Status on 11 November 2026: **design only. No firmware for this node was found in GitHub.**
This document records what was checked, what is missing, and what must be true before the
node can be called supported. It does not replace or change any existing firmware.

## What was looked for and where

| Checked | Result |
|---|---|
| `main` (`c23a801`), `firmware/c3-lamp-node/` | An older C3 lamp, firmware 0.2.1, marked HISTORICAL in its own README |
| All 77 remote branch names for "lamp", "lantern", "c3" | `cursor/fix-lamp-desk-freeze`, `integration/s3-lamp-fastled-optin-20261102` (both S3), `chatgpt/status-colour-c3-recovery` (not a lamp) |
| `firmware/s3-lamp-node/` | Active S3 carbide lamp, firmware 0.4.x |

If the simplified C3 firmware exists, it is on a local machine and has not been pushed.
**Pending item 1: push it to a branch, untouched, before any integration work.**

## The three lamps

| | Simplified C3 (design, as described by the owner) | C3 in this repository (0.2.1, historical) | S3 in this repository (active) |
|---|---|---|---|
| Board | ESP32-C3 SuperMini | ESP32-C3 Super Mini **with OLED** (HUNT board); OLED failure is a fault | ESP32-S3 Dev Module family |
| Pixels | 7 NeoPixels | 7 WS2812 on GPIO2 | 7-pixel jewel |
| Controls | One button | Two buttons (GPIO9, GPIO0) | Striker button, blow sensor |
| Ignite | Hold 2 s, only when permitted | Long press is 650 ms; local test, no permission step | Strike; 650 ms long press |
| Extinguish | Short press | Local stop | Blow or command |
| Ignition and shutdown | Sputter in and out | Carbide effects include sputter | Carbide machine with audio |
| Comms loss | Fade off | Goes to safe idle when show-controlled and authority is stale | Same shared rule |
| Operator STOP | Immediate OFF and lockout | `LAMP:STOP` goes idle; **no lockout** | To be confirmed on source |
| After emergency or STOP | No automatic recovery; P4 releases and re-authorises | `EMERGENCY:CLEAR` returns the node to standalone, where local control is allowed again | Same shared state model |
| Identity | Persistent node identity | No stored identity found | Logical `LAMP-01` / `LAMP-02` in NVS |
| Transport | ESP-NOW | ESP-NOW, channel 1 | ESP-NOW via Communications S3 |

Rows marked from the owner's description are **not verified**: there is no source to read.

## Answers to the six questions

1. **Is the firmware in GitHub?** No. Only the older OLED-based C3 lamp is.
2. **Is a newer local-only implementation absent?** It appears so. Nothing matching the
   described behaviour (2 s hold, STOP lockout, permission to ignite) exists on any branch.
3. **Does the P4 support its registration and permissions?** Partly.
   `nodes/LampNodeLink.cpp` tracks a lamp's logical ID, sends `LAMP:NODE:OWN:GRANT` as the
   ownership keep-alive and relays `EMERGENCY:STOP` / `EMERGENCY:CLEAR`. It has **no command
   that permits or forbids manual ignition, no STOP lockout, and no separate release or
   re-authorise step**. `EMERGENCY:CLEAR` is the only release, and the shared state model
   then allows local control again.
4. **Can Studio represent its cues?** Not from the website. The SHDO compiler accepts
   `lamp` clips on a device with route `lamp-node` and emits `LAMP:NODE:<id>:IGNITE`,
   `EXTINGUISH` and `FX:<name>:BRI=<n>`. Website Studio has no Lamp cue type, so it cannot
   author them. No remote colour-transition command exists in `showduino_lamp_node.h`
   beyond `SOLID`, `FX` and `BRIGHTNESS`.
5. **Are S3 lamp commands compatible?** The wire vocabulary in
   `protocol/showduino_lamp_node.h` is shared, so `IGNITE`, `EXTINGUISH`, `FX`, `SOLID`,
   `BRIGHTNESS`, `OWN:GRANT`, `EMERGENCY:STOP` and `EMERGENCY:CLEAR` would parse on both.
   The nodes must still be told apart: the S3 advertises audio, blow, light and voltage
   capabilities the C3 does not have, and the described C3 safety behaviour (lockout,
   permission) is stricter than the shared state model. **A capability or model token in
   the node's announce is needed so the P4 and Director never assume S3 features on a C3.**
6. **Is its safety behaviour independently enforced?** Unknown for the simplified node.
   For the repository C3: emergency is handled on the node, and comms loss sends a
   show-controlled lamp to safe idle without the P4. There is no lockout to enforce.

## Requirements the simplified node must meet (acceptance, on hardware)

Each needs a bench result with firmware commit, date and observer. None has one.

| # | Requirement | How to observe |
|---|---|---|
| L1 | Manual ignition is refused unless the P4 has granted permission | Hold button 2 s with no grant: stays dark; serial shows the refusal reason |
| L2 | Ignition needs a 2 s hold; a shorter press never ignites | Presses of 0.5 s, 1.5 s, 2.1 s |
| L3 | Short press extinguishes with a sputter-out | Lit lamp, tap button |
| L4 | Operator STOP gives immediate OFF and lockout | STOP from Director: dark within one frame; button does nothing |
| L5 | Emergency gives immediate OFF (or the defined emergency state) and lockout | Assert emergency: same; button does nothing |
| L6 | No automatic recovery after STOP or emergency clear | Clear emergency: lamp stays dark and locked until the P4 re-authorises |
| L7 | Comms loss fades the lamp off on the node's own timer | Power off the Communications S3 while lit: fades off without any command |
| L8 | Reboot keeps identity and comes up dark and locked | Power-cycle the node |
| L9 | A remote colour or effect change is a controlled transition, not a jump | Send two effects in sequence |
| L10 | The lamp never lights during emergency whatever is sent | Send `IGNITE`, `FX`, `SOLID` during emergency: all refused |

## Pending work, in order

1. Owner: push the simplified C3 firmware to a branch as it is.
2. Read that source against L1–L10 and mark which are implemented.
3. Decide the wire additions (owner decision, protocol change, needs its own PR):
   a permit/forbid manual ignition command, a lockout state in status, a release or
   re-authorise command distinct from `EMERGENCY:CLEAR`, and a model or capability token.
4. P4 `LampNodeLink`: implement those, with host tests beside `tools/lamp-node-tests`.
5. Website Studio: add a Lamp cue type only after 3 and 4, with parity fixtures in
   `tools/production-tests`.
6. Bench test L1–L10.

Until item 1, nothing further can be implemented or verified. The existing
`firmware/c3-lamp-node/` tree stays as it is.
