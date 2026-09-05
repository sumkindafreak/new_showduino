# Showduino Architecture

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

### Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Official role: single source of truth for show state and show services |
| **Stage Controller** | Physical ESP32-P4 product that runs the Show Engine |
| ~~Stage Engine~~ | **Retired** — do not use |

Firmware path `firmware/stage-engine-p4/` is the current Show Engine sketch location. It is planned to be renamed later; documentation already uses **Show Engine**.

### Non-negotiable ownership

- The Show Engine owns authoritative show state, safety policy, primary project/configuration storage, Web UI, Web API, and WebSocket state.
- The Communications Engine owns ESP‑NOW transport and UART to the Show Engine. It must **not** make show-level decisions, run the timeline, or invent output state. Wi‑Fi and BLE on this role are **FUTURE / RESERVED / NOT IMPLEMENTED**.
- The Director commands and displays. It does **not** host or proxy the primary Web UI. USB is not the normal Director link.
- Nodes act. Application addressing uses **logical Showduino device IDs**, not MAC addresses at the application layer.
- Relay requests use **absolute** ON/OFF (and timed pulse) states — not distributed `TOGGLE`.
- Command **acceptance** and physical **action completion** are separate events.
- A running show must not depend on an active Director, browser session, Wi‑Fi client association, or internet.

### Maturity (do not overstate)

The Show Engine owns the Stage 3 state model and the newer Stage 6 authoritative `ShowRuntime`. It discovers and validates versioned productions from P4 SD, transactionally loads TEST/LOG cues into the RAM timeline, publishes state/snapshots, and runs ordered cue dispatch with pause/resume, finish, and emergency interruption handling. Broader asset/project management, physical cue engines, DMX, and Web UI remain incomplete.

Director display of relays/show/emergency follows confirmed `STATE:*` (and snapshot sync). Optimistic relay TOGGLE UI is removed.

Director SD show/storage features that exist today are **current implementation details**, not the final home of authoritative projects.

---

## Canonical communication paths

### Director path (current live design)

```text
Director ESP32-S3
    → ESP-NOW
ESP32-S3 Comms Controller
    → UART 115200 8N1
Show Engine ESP32-P4 (Stage Controller)
```

### Node path

```text
Showduino Node
    → ESP-NOW
ESP32-S3 Comms Controller
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Browser / phone path (conceptual target)

```text
Phone / Tablet / Laptop
    → Wi-Fi (FUTURE / RESERVED / NOT IMPLEMENTED on the S3 Comms Controller)
    → Show Engine services
```

Do **not** describe the Director as the normal Web UI host or proxy.

```text
┌──────────────────────────┐
│ Director (ESP32-S3)      │
│ Commands + displays      │
└────────────┬─────────────┘
             │ ESP-NOW
             ▼
┌──────────────────────────┐
│ Communications Engine    │
│ (ESP32-S3 Dev Module)    │
│ ESP-NOW + UART           │
└────────────┬─────────────┘
             │ UART
             ▼
┌──────────────────────────┐
│ Show Engine              │
│ Stage Controller (P4)    │
│ Decides + stores + serves│
└────────────┬─────────────┘
             │ UART (to Comms) then ESP-NOW
             ▼
┌──────────────────────────┐
│ Nodes (relay, …)         │
│ Act                      │
└──────────────────────────┘
```

---

## Role details

### 1. Director (ESP32-S3)

**Active firmware:** `firmware/director-esp32-8048s050/`

Hardware: 5″ 800×480 touchscreen (e.g. ESP32-8048S043/S050), PSRAM, SD for UI assets and temporary local data.

Responsibilities:

- Operator touch UI (LVGL)
- Emit **requests** (show control, cues, absolute relay states, emergency)
- Display status only after authoritative Show Engine updates (target behaviour)
- Diagnostics for link and local hardware

Not responsibilities:

- Primary Web UI / Web API
- Authoritative project library (long-term)
- Timeline execution
- Direct control of node GPIOs

UART from the Director to the P4 is **not** the normal product path (optional bench/service only).

### 2. Communications Engine (dedicated ESP32-S3)

**Active firmware:** `firmware/s3-comms-controller/`

Hardware: standard USB-programmable ESP32-S3 Dev Module. Application link to the P4 is UART (P4 GPIO4 RX / GPIO5 TX, S3 GPIO17 TX / GPIO18 RX). Native USB on the S3 is for programming/debug only.

Responsibilities:

- ESP‑NOW desk link to the Director
- UART framing/forwarding to the Show Engine
- Remember Director MAC; return P4 newline-framed status over ESP-NOW
- Device connection monitoring and transport health

Must not: run shows, own show state, host SoftAP/WebUI, initialise BLE, install ESP-Hosted, or return **false success** for unimplemented routes.

The Waveshare onboard ESP32-C6 is **UNUSED BY SHOWDUINO / RESERVED HARDWARE**. Do not flash it or wait for it.

The previous external Communications Engine (`firmware/c3-supermini-espnow-bridge/`, ESP32-C3 SuperMini / SUE) is **LEGACY / SUPERSEDED**.

**FUTURE / RESERVED / NOT IMPLEMENTED** on this S3: Bluetooth LE, Wi-Fi SoftAP/STA networking, WebUI proxy, OTA.

### 3. Show Engine on Stage Controller (ESP32-P4)

**Active firmware:** `firmware/stage-engine-p4/` (folder name legacy; role = Show Engine)

Responsibilities (target):

- Authoritative show/runtime/emergency state
- Timeline and cue scheduling
- Project storage and configuration
- Validate requests; publish state changes
- Dispatch node commands by logical device ID
- Local show outputs where fitted (DMX, pixels, audio) when implemented
- Web UI / Web API / WebSocket

**Today:** command hub plus authoritative runtime and RAM-backed Stage 6 timeline execution. It parses requests, applies the emergency gate, publishes runtime/state, dispatches loaded cues, and transactionally loads versioned TEST/LOG productions from P4 SD. Deeper output/service capabilities remain planned. An experimental relay route is retained in source but is not part of the supported current stack.

### 4. Nodes

**Experimental / future example:** `firmware/relay-node-esp32/`

The current supported stack ends at the P4. Future nodes will execute absolute commands and report results; the retained relay source is a prototype, not a supported product path. Expandable family: audio, lighting, sensor, motor, environmental, etc.

---

## Canonical active firmware

```text
firmware/director-esp32-8048s050/
firmware/s3-comms-controller/             # Communications Engine (ESP32-S3)
firmware/stage-engine-p4/                 # Show Engine (rename planned)
```

### Other trees (not the active product path)

| Path | Classification |
|------|----------------|
| `firmware/relay-node-esp32/` | Experimental / future — retained prototype, not the supported current stack |
| `firmware/p4-c6-espnow-bridge/` | Unused / reserved — onboard C6, not current application firmware |
| `firmware/c3-supermini-espnow-bridge/` | Legacy / superseded — previous external C3 / SUE Communications Engine |
| `firmware/director-s3/` | Legacy / experimental |
| `firmware/espnow-bridge/` | Legacy scaffold |
| `firmware/controller-cyd/` | Legacy — archive candidate |
| `firmware/executor-mega/` | Legacy — archive candidate |
| `firmware/touch-probe-8048/` | Diagnostic |
| `.../ShowduinoSdTouchTest/` | Diagnostic |
| `firmware/sue-esp32s3-node/` | Incomplete |

---

## Example request lifecycle (target)

```text
Director → SHOW_START_REQUEST (via ESP-NOW)
Communications Engine → UART to Show Engine
Show Engine validates → state PLAYING
Show Engine publishes SHOW_STATE_CHANGED
Communications Engine → Director (and Web clients when Wi-Fi exists)
Director / browser display PLAYING
```

Future relay lifecycle example (absolute state; not yet a supported product path):

```text
Director → relay channel N requested ON (by device ID)
Show Engine accepts or rejects
Communications Engine delivers node command
Node completes action → result
Show Engine publishes authoritative output/node state
Director displays confirmed state
```

Placeholder pixel/audio routes must **not** report success when no real action occurred.

---

## UART (Communications Engine ↔ Show Engine)

```text
Dedicated ESP32-S3 Comms Controller  ↔  Show Engine ESP32-P4
Baud: 115200 8N1, newline-framed ASCII
P4 GPIO4 RX / GPIO5 TX  (S3 GPIO17 TX / GPIO18 RX)
GND shared
```

Factory P4↔C6 SDIO is **not** the current command transport. Pin details: [`docs/final-hardware-architecture.md`](final-hardware-architecture.md). The Director does not sit on this UART in the normal product path.

---

## Safety model

Emergency stop overrides entertainment commands.

When the Show Engine accepts an emergency activate request it must:

- Enter emergency locked state
- Pause the active timeline while preserving its position for an explicit operator resume
- Command dangerous outputs and nodes toward safe states
- Keep status reporting available
- Require an explicit clear / authorised reset policy before normal operation

Nodes should fail safe and remain safe until a valid clear is applied per policy.

A lost Director or browser must **not** by itself stop a running show, unless a configured safety policy says otherwise for that installation.

---

## Storage (target vs today)

| Store | Target owner | Today |
|-------|--------------|-------|
| Projects, shows, configuration | Show Engine | Early / incomplete on P4; Director SD has temporary show/config helpers |
| UI assets for the desk | Director | Director SD |
| Runtime media for execution | Show Engine (+ nodes as needed) | Mostly unimplemented |

---

## Why this architecture

- Deterministic show authority on one engine
- Wireless operators and nodes without long cable plants
- Transport failures degrade communications, not necessarily local show execution
- Web and Director are clients of the same SoT
- Hardware can change without rewriting show meaning

---

## Development status relative to this document

**Aligned:** role names in this doc; live Director→ESP‑NOW→S3 Comms Controller→UART→P4 path; current supported stack ends at the P4; Comms must not own show logic.

**Still firmware debt:** `TOGGLE` remains in compatibility paths; node routing is MAC-centric; stub routes can look like success; Show Engine Web UI is not on P4; Director SD still has legacy show-package helpers; folder is still named `stage-engine-p4`.
