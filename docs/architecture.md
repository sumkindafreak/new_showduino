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
- The Communications Engine owns Wi‑Fi and ESP‑NOW transport (and UART to the Show Engine). It must **not** make show-level decisions, run the timeline, or invent output state.
- The Director commands and displays. It does **not** host or proxy the primary Web UI. USB is not the normal Director link.
- Nodes act. Application addressing uses **logical Showduino device IDs**, not MAC addresses at the application layer.
- Relay requests use **absolute** ON/OFF (and timed pulse) states — not distributed `TOGGLE`.
- Command **acceptance** and physical **action completion** are separate events.
- A running show must not depend on an active Director, browser session, Wi‑Fi client association, or internet.

### Maturity (do not overstate)

The Show Engine now owns an explicit Stage 3 runtime state model (show IDLE/PLAYING/EMERGENCY, emergency CLEAR/ACTIVE, per-channel relay knowledge, relay-node availability) and publishes `STATE:*` / snapshots. It does **not** yet contain timeline execution, project storage migration, DMX, pixel, audio, or Web UI.

Director display of relays/show/emergency follows confirmed `STATE:*` (and snapshot sync). Optimistic relay TOGGLE UI is removed.

Director SD show/storage features that exist today are **current implementation details**, not the final home of authoritative projects.

---

## Canonical communication paths

### Director path (current live design)

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Node path

```text
Showduino Node
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Browser / phone path (conceptual target)

```text
Phone / Tablet / Laptop
    → Wi-Fi
Communications Engine ESP32-C3
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
┌──────────────────────────┐     Wi-Fi (planned)
│ Communications Engine    │◄──────────────── Phone / tablet / laptop
│ (ESP32-C3)               │
│ ESP-NOW + UART (+ Wi-Fi) │
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

### 2. Communications Engine (ESP32-C3)

**Active firmware:** `firmware/c3-supermini-espnow-bridge/`

Responsibilities:

- ESP‑NOW desk link to the Director
- ESP‑NOW fabric to nodes
- UART framing/forwarding to the Show Engine
- Device connection monitoring and transport health
- Planned: Wi‑Fi AP/STA so browsers reach Show Engine services

Must not: run shows, own show state, control DMX/pixels/audio as an authority, or return **false success** for unimplemented routes.

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

**Today:** early command hub (parse requests, emergency gate, route relay work through the Communications Engine, return simple ACK/status lines). Treat deeper capabilities as **planned**.

### 4. Nodes

**Active example:** `firmware/relay-node-esp32/`

Nodes execute absolute commands and report results. Expandable family: audio, lighting, sensor, motor, environmental, etc.

---

## Canonical active firmware

```text
firmware/director-esp32-8048s050/
firmware/c3-supermini-espnow-bridge/
firmware/stage-engine-p4/                 # Show Engine (rename planned)
firmware/relay-node-esp32/
```

### Other trees (not the active product path)

| Path | Classification |
|------|----------------|
| `firmware/director-s3/` | Legacy / experimental |
| `firmware/p4-c6-espnow-bridge/` | Experimental / incomplete |
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

Relay example (absolute state):

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
Communications Engine ESP32-C3  ↔  Show Engine ESP32-P4
Baud: 115200 (current sketches)
GND shared
```

Pin details live in the active firmware READMEs / sketches. The Director does not sit on this UART in the normal product path.

---

## Safety model

Emergency stop overrides entertainment commands.

When the Show Engine accepts an emergency activate request it must:

- Enter emergency locked state
- Stop or freeze show timelines (when implemented)
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

**Aligned:** role names in this doc; live Director→ESP‑NOW→C3→UART→P4 path; relay nodes off the P4; Comms must not own show logic.

**Still firmware debt (docs only — not fixed here):** optimistic Director relay UI; `TOGGLE` still present in places; MAC-centric node config; stub routes that can look like success; Show Engine Web UI not on P4; Director SD still holding show packages; folder still named `stage-engine-p4`.
