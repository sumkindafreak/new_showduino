# Showduino v1

**Showduino** is a modular, distributed show-control platform for scare attractions, escape rooms, immersive experiences, and interactive props.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

- The **Show Engine** is the single source of truth for show state, safety policy, project storage, configuration, Web UI, Web API, and WebSocket state.
- The **Communications Engine** provides Wi‑Fi and ESP‑NOW transport. It must not make show-level decisions.
- The **Director** is an operator interface only. It does **not** host or proxy the primary Web UI.
- A running show must **not** depend on an active Director, browser, Wi‑Fi client, or internet connection.
- Application code addresses devices by **logical Showduino device IDs**, not MAC addresses (transport may resolve IDs internally).
- Relay requests use **absolute states** (ON/OFF), not distributed `TOGGLE`.
- **Command acceptance** and **physical action completion** are separate lifecycle events.
- USB is **not** the normal Director communication path (diagnostics / recovery only, if used later).

### Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Official processor role: authoritative show controller software |
| **Stage Controller** | Physical ESP32-P4 product that runs the Show Engine |
| ~~Stage Engine~~ | **Retired** — do not use in new documentation |

The firmware folder `firmware/stage-engine-p4/` remains temporarily for compatibility. Documentation treats it as the **Show Engine**. A rename is planned later.

## Documentation

| Document | Contents |
|----------|----------|
| [`docs/constitution.md`](docs/constitution.md) | Permanent architectural rules |
| [`docs/architecture.md`](docs/architecture.md) | System architecture and maturity |
| [`docs/repository-status.md`](docs/repository-status.md) | Firmware classification (ACTIVE / LEGACY / …) |
| [`docs/final-hardware-architecture.md`](docs/final-hardware-architecture.md) | Hardware topology and board roles |

## Canonical communication paths

### Director

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Nodes

```text
Showduino Node
    → ESP-NOW
Communications Engine ESP32-C3
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Phone / browser (conceptual target)

```text
Phone / Tablet / Laptop
    → Wi-Fi
Communications Engine ESP32-C3
    → Show Engine services (Web UI / Web API / WebSocket)
```

The Director is **not** a normal Web UI host or proxy.

## Core roles

### Show Engine (ESP32-P4 Stage Controller)

Owns authoritative:

- Show / timeline / cue state
- Project and asset storage (primary)
- Safety and emergency policy
- Node command dispatch and result handling
- Web UI, Web API, WebSocket state, configuration

**Maturity:** Current firmware under `firmware/stage-engine-p4/` is an **early command hub**. It does **not** yet contain the full planned timeline engine, project storage, DMX, pixel, audio, or Web UI/API. Do not overstate capability.

### Communications Engine (ESP32-C3)

- ESP‑NOW with the Director
- ESP‑NOW with Showduino nodes
- UART with the Show Engine
- Planned: Wi‑Fi AP/STA for browser access to Show Engine services

Transports only. Does not run the timeline or invent show state.

### Director (ESP32-S3 touchscreen)

- Show selection and start/stop/pause/resume **requests**
- Manual cue **requests**
- Status, timeline, node, fault, and emergency **display**
- Local UI preferences / temporary SD features as implementation details

A Director command is a **request**. Successful display of a state change requires authoritative confirmation from the Show Engine. Existing Director SD show/storage features are **not** the final location of authoritative projects.

### Nodes

Specialist devices (relay, audio, lighting, sensor, motor, etc.) that **act** on commanded absolute states and report results.

## Firmware map (summary)

**ACTIVE** (canonical runtime):

```text
firmware/director-esp32-8048s050/          Director (ESP32-S3)              [ACTIVE]
firmware/c3-supermini-espnow-bridge/       Communications Engine (ESP32-C3) [ACTIVE]
firmware/stage-engine-p4/                  Show Engine / Stage Controller    [ACTIVE]
firmware/relay-node-esp32/                 Relay Node                        [ACTIVE]
```

**Other** (not the supported production stack):

```text
firmware/director-s3/                      [LEGACY]
firmware/espnow-bridge/                    [LEGACY]
firmware/p4-c6-espnow-bridge/              [EXPERIMENTAL]
firmware/touch-probe-8048/                 [DIAGNOSTIC]
firmware/sue-esp32s3-node/                 [INCOMPLETE]
firmware/controller-cyd/                   [ARCHIVE CANDIDATE]
firmware/executor-mega/                    [ARCHIVE CANDIDATE]
```

Full table, ownership boundaries, archive proposal, and naming debt: [`docs/repository-status.md`](docs/repository-status.md).

## Repository layout

```text
firmware/     MCU sketches (classified in docs/repository-status.md)
web/          GoreFX dashboard / Scene Manager (host-side; not yet the live Show Engine Web UI)
docs/         Constitution, architecture, repository status, hardware
```

## Current progress (honest)

- Live transport path: Director ↔ ESP‑NOW ↔ C3 ↔ UART ↔ P4, with C3 also bridging ESP‑NOW nodes
- Early Show Engine command parsing, emergency gate, and relay routing via Communications Engine
- Director LVGL UI and ESP‑NOW client
- Relay Node ESP‑NOW actuator
- Host-side GoreFX / Scene Manager prototypes under `web/`

**Not yet:** full Show Engine timeline, primary project store on P4, Show Engine Web UI/API, Communications Engine Wi‑Fi front door, shared protocol package, ACK-driven Director relay display, logical device-ID addressing throughout firmware.

## Long-term vision

Hardware may evolve; roles stay fixed. Shows are authored and stored against the Show Engine. Operators use the Director and/or browsers. Nodes remain replaceable specialists on the same fabric.
