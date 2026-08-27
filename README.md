# Showduino v1

**Showduino** is a modular, distributed show-control platform for scare attractions, escape rooms, immersive experiences, and interactive props.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

- The **Show Engine** is the single source of truth for show state, safety policy, project storage, configuration, Web UI, Web API, and WebSocket state.
- The **Communications Engine** provides ESP‑NOW and UART transport. It must not make show-level decisions. The current hardware is a dedicated ESP32-S3 Comms Controller.
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
| [`docs/final-hardware-architecture.md`](docs/final-hardware-architecture.md) | Current hardware topology, P4 pin map, UART wiring |
| [`docs/hardware-pinout.md`](docs/hardware-pinout.md) | Current P4 resource and pin map |
| [`docs/hardware-baseline-2026-08-25.md`](docs/hardware-baseline-2026-08-25.md) | Board-capability baseline (C6 now unused/reserved) |
| [`docs/audio-pixel-engine.md`](docs/audio-pixel-engine.md) | Show audio, system audio and pixel architecture |
| [`docs/future-p4-c6-sdio-transport.md`](docs/future-p4-c6-sdio-transport.md) | Future SDIO / ESP-Hosted notes (not implemented) |
| [`docs/plugin-bus.md`](docs/plugin-bus.md) | Showduino Plug-in Bus (I²C) |
| [`docs/creating-showduino-i2c-plugin.md`](docs/creating-showduino-i2c-plugin.md) | Adding an I²C plugin definition or driver |

## Canonical communication paths

### Director

```text
Director ESP32-S3
    → ESP-NOW
ESP32-S3 Comms Controller
    → UART 115200 8N1
Show Engine ESP32-P4 (Stage Controller)
```

### Nodes

```text
Showduino Node
    → ESP-NOW
ESP32-S3 Comms Controller
    → UART
Show Engine ESP32-P4 (Stage Controller)
```

### Phone / browser (conceptual target)

```text
Phone / Tablet / Laptop
    → Wi-Fi (FUTURE / RESERVED / NOT IMPLEMENTED on the S3 Comms Controller)
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

### Communications Engine (dedicated ESP32-S3)

- ESP‑NOW with the Director
- UART with the Show Engine (P4 GPIO4/5; S3 GPIO17 TX / GPIO18 RX)
- Must not host SoftAP/WebUI, BLE, or OTA in this phase
- Must not run the timeline or invent show state

The Waveshare onboard ESP32-C6 is **UNUSED BY SHOWDUINO / RESERVED HARDWARE**. Do not flash or require it.

The previous external Communications Engine was an ESP32-C3 SuperMini (`firmware/c3-supermini-espnow-bridge/`). It remains in-tree as **LEGACY / SUPERSEDED**.

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
firmware/s3-comms-controller/              Communications Engine (ESP32-S3) [ACTIVE]
firmware/stage-engine-p4/                  Show Engine / Stage Controller    [ACTIVE]
firmware/relay-node-esp32/                 Relay Node                        [ACTIVE]
```

**Other** (not the supported production stack):

```text
firmware/p4-c6-espnow-bridge/              [UNUSED / RESERVED] onboard C6
firmware/c3-supermini-espnow-bridge/       [LEGACY / SUPERSEDED] previous external C3 / SUE
firmware/director-s3/                      [LEGACY]
firmware/espnow-bridge/                    [LEGACY]
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

- Live transport path: Director ↔ ESP‑NOW ↔ dedicated ESP32-S3 Comms Controller ↔ UART ↔ P4. Onboard C6 is unused/reserved. Factory P4↔C6 SDIO and ESP-Hosted are not used.
- Early Show Engine command parsing, emergency gate, and relay routing via Communications Engine
- Director LVGL UI and ESP‑NOW client
- Relay Node ESP‑NOW actuator
- Host-side GoreFX / Scene Manager prototypes under `web/`

**Not yet:** full Show Engine timeline, primary project store on P4, Show Engine Web UI/API, Communications Engine Wi‑Fi front door, shared protocol package, ACK-driven Director relay display, logical device-ID addressing throughout firmware.

## Long-term vision

Hardware may evolve; roles stay fixed. Shows are authored and stored against the Show Engine. Operators use the Director and/or browsers. Nodes remain replaceable specialists on the same fabric.
