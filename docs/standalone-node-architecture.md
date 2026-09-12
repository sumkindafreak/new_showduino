# Showduino standalone specialist nodes

Current architecture extension, 2026-09-07. Specialist nodes may run **managed under the P4** or as **standalone products with a local WebUI**. The P4 remains authoritative whenever Showduino owns the node.

```text
Director  →  Comms S3  →  P4 (authoritative)
                              │
                              │ UART → Comms S3 → ESP-NOW
                              ▼
                     Audio / Lamp / (later Pixel)
                     optional SoftAP WebUI on ESP-NOW channel 1
```

Random ESP-NOW presence is **not** ownership. Valid Showduino authority is a **P4 GRANT** forwarded by the Communications S3:

```text
AUDIO:NODE:OWN:GRANT
AUDIO:OWN:GRANT
OWN:GRANT
```

The P4 sends GRANT when the node announces, and retries GRANT about every 2 s as keepalive. `STATUS` / inventory may **keep** an existing grant. They must not create one.

## Modes

| Mode | Meaning |
|------|---------|
| SEARCHING | Boot-safe hardware, ESP-NOW up, waiting ~8 s for GRANT |
| STANDALONE | No grant. SoftAP + full local WebUI |
| SHOW_CONTROLLED | P4 owns the node. SoftAP stays up as **status / diagnostics only** |
| EMERGENCY | Above everything. Pixel-capable outputs go bright white. Audio stops and mutes |
| FAULT | Node-level fault |

On GRANT: Audio and Pixel stop autonomous actions and go to safe idle. They do **not** restore the previous local FX / audio. They wait for a **fresh** P4 command.

The S3 Lamp Node is the interactive-prop exception: GRANT enters SHOWDUINO mode and **keeps the current flame**. It does not apply stale P4 FX. A later P4 command becomes authoritative.

On GRANT loss: apply the node-type fail-safe, then STANDALONE WebUI. A short ESP-NOW gap inside the keepalive window is not standalone permission.

Emergency CLEAR returns to safe idle. It does not resume the previous cue.

## SoftAP

SSID is `Showduino-<Type>-<MAC suffix>`, for example `Showduino-Audio-31C8`. The S3 Lamp Node uses the logical Node ID instead: `Showduino-Lamp-LAMP-01`. Password `showduino`. Channel **must** remain `SHOWDUINO_ESPNOW_CHANNEL` (1). Radio mode is AP+STA so the local WebUI cannot retune ESP-NOW. Node SoftAP uses **192.168.5.1** so it does not collide with the Communications S3 Studio AP at 192.168.4.1. ESP-NOW stays on STA; SoftAP must not start until ESP-NOW is up, and must rebind ESP-NOW after the AP comes up. STA scanning is disabled. TX power is held down so a node AP cannot deafen Director ↔ Comms.

Persist **name / defaults only**. Never persist live ON / FX / PLAY as a boot state.

When SHOW_CONTROLLED, the WebUI shows **CONTROLLED BY SHOWDUINO**. Hiding buttons is not enough: firmware must reject theatrical commands from WEB and LOCAL origins.

## Fail-safe by node type

| Node | Authority loss / comms-loss / P4 return |
|------|------------------------------------------|
| Audio | Stop, mute, idle. Do not resume |
| Lamp | Pixels off. Do not restore FX |
| Pixel (later) | Blackout |
| MOSFET | **All outputs OFF. Never restore. No schedules. No persisted ON.** |

MOSFET is specified for the shared ownership model but is **not implemented** in this phase. It has a hard no-autonomy safety exception: boot, comms-loss, fault, emergency, and P4-return are all ALL OFF.

## Current firmware

- Audio Node `0.4.1` — ownership + SoftAP WebUI implemented beside playback states (`IDLE` / `PLAYING` / … stay playback-only).
- S3 Lamp Node `0.3.1` — same GRANT machine; standalone carbide lamp + local WebUI; P4 `LampNodeLink` and ESP-NOW FX implemented. Historical C3 Lamp Node remains reference-only.
- MOSFET / C3 Pixel — Pixel firmware exists; MOSFET is document only.
