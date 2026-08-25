# Showduino v1

**Showduino** is a modular, distributed show-control platform for scare attractions, escape rooms, immersive experiences, and interactive props.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

- The **Show Engine** is the single source of truth for show state, safety policy, project storage, configuration, Web UI, Web API, and WebSocket state.
- The **Communications Engine** provides Wi-Fi / ESP-NOW / wireless transport. It must not make show-level decisions.
- The **Director** is an operator interface only.
- A running show must **not** depend on an active Director, browser, Wi-Fi client, or internet connection.
- Application code addresses devices by logical Showduino device IDs, not MAC addresses at the application layer.
- Relay requests use absolute states (ON/OFF), not distributed `TOGGLE`.
- Command acceptance and physical action completion are separate lifecycle events.

## Hardware baseline — 25 August 2026

The canonical Stage Controller hardware is now the **Waveshare ESP32-P4-Module-DEV-KIT** using its onboard capabilities wherever practical.

```text
Director ESP32-S3
    → ESP-NOW
Onboard ESP32-C6
    → integrated P4/C6 transport (target: board SDIO path)
ESP32-P4 Show Engine / Stage Controller
```

The Stage Controller baseline now uses:

- **ESP32-P4** as the Show Engine.
- **Onboard ESP32-C6** as the Communications Engine hardware target.
- **ESP32-P4 RTC + Waveshare rechargeable RTC battery connection** instead of an external DS3231.
- **Onboard ES8311 + NS4150B speaker path** for Showduino/system sounds.
- **External PCM5102A** retained for dedicated show/programme audio.
- Onboard microSD, Ethernet and USB.
- Emergency NeoPixel on **GPIO24**.
- Momentary emergency push button on **GPIO25**.

The separate external C3/SUE communications board and external DS3231 RTC are no longer part of the physical Stage Controller baseline.

Full decision record: [`docs/hardware-baseline-2026-08-25.md`](docs/hardware-baseline-2026-08-25.md).

## Important firmware transition note

The **hardware target has moved ahead of the current communications firmware**.

The repository still contains the previously working external ESP32-C3/UART bridge and its DS3231 time-service code. That code is retained as a compatibility/rollback reference until the onboard C6 implementation has feature parity.

Do not describe the current `p4-c6-espnow-bridge` sketch as production-ready yet: it is the bring-up target and still needs the final P4↔C6 transport implementation, bidirectional state/replies, node routing and WebUI transport qualification.

## Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Authoritative show-controller software role |
| **Stage Controller** | Physical ESP32-P4 product that runs the Show Engine |
| **Communications Engine** | Wireless transport role; hardware target is the Stage Controller's onboard ESP32-C6 |
| **Director** | ESP32-S3 touchscreen operator interface |
| **SUE** | Historical name; no longer requires a separate physical board |
| ~~Stage Engine~~ | Retired term |

The firmware folder `firmware/stage-engine-p4/` remains temporarily for compatibility.

## Documentation

| Document | Contents |
|----------|----------|
| [`docs/constitution.md`](docs/constitution.md) | Permanent architectural rules |
| [`docs/architecture.md`](docs/architecture.md) | Current architecture and migration boundary |
| [`docs/hardware-baseline-2026-08-25.md`](docs/hardware-baseline-2026-08-25.md) | Lean P4 hardware decision record |
| [`docs/final-hardware-architecture.md`](docs/final-hardware-architecture.md) | Physical board/module roles |
| [`docs/hardware-pinout.md`](docs/hardware-pinout.md) | Current P4 resource and pin map |
| [`docs/audio-pixel-engine.md`](docs/audio-pixel-engine.md) | Show audio, system audio and pixel architecture |
| [`docs/repository-status.md`](docs/repository-status.md) | Firmware classification and migration status |

## Canonical target communication paths

### Director

```text
Director ESP32-S3
    → ESP-NOW
Communications Engine — onboard ESP32-C6
    → integrated Stage Controller transport
Show Engine — ESP32-P4
```

### Nodes

```text
Showduino Node
    → ESP-NOW
Communications Engine — onboard ESP32-C6
    → integrated Stage Controller transport
Show Engine — ESP32-P4
```

### Phone / browser

```text
Phone / Tablet / Laptop
    → Wi-Fi
Communications Engine — onboard ESP32-C6
    → Show Engine Web services on ESP32-P4
```

The Director is not the normal Web UI host or proxy.

## Core roles

### Show Engine — ESP32-P4 Stage Controller

Owns authoritative:

- Show / timeline / cue state
- Project and asset storage
- Safety and emergency policy
- Node command dispatch and result handling
- Web UI, Web API, WebSocket state and configuration
- Local Stage Controller outputs as implemented

### Communications Engine — onboard ESP32-C6

Target responsibilities:

- ESP-NOW with Director
- ESP-NOW with Showduino nodes
- Wi-Fi AP/STA for browser access
- Transport between the C6 radio side and the P4 Show Engine

It transports only. It does not run the timeline or invent show state.

### Director — ESP32-S3 touchscreen

- Show selection and show-control requests
- Manual cue requests
- Status, timeline, node, fault and emergency display
- Local UI preferences and diagnostics

A Director command is a request. State changes are displayed only after authoritative confirmation from the Show Engine.

### Nodes

Specialist devices (relay, audio, lighting, sensor, motor, etc.) that act on commanded states and report results.

## Audio roles

Showduino now has two local audio **roles** on the Stage Controller:

```text
Onboard ES8311 + NS4150B
    → system / boot / ready / warning sounds

External PCM5102A
    → show music / dialogue / ambience / timed SFX
```

The ESP32-P4 exposes one I2S peripheral, so v1 firmware must arbitrate that shared resource. Do not assume the two physical audio outputs can run independent simultaneous streams until that behaviour is deliberately implemented and tested.

## Firmware map

**ACTIVE / authoritative runtime pieces:**

```text
firmware/director-esp32-8048s050/          Director
firmware/stage-engine-p4/                  Show Engine / Stage Controller
firmware/relay-node-esp32/                 Relay Node
```

**TARGET migration:**

```text
firmware/p4-c6-espnow-bridge/              Onboard C6 Communications Engine bring-up
```

**COMPATIBILITY / rollback:**

```text
firmware/c3-supermini-espnow-bridge/       Previous external C3 Communications Engine
```

Other legacy, diagnostic and incomplete trees are classified in [`docs/repository-status.md`](docs/repository-status.md).

## Current progress

- P4 Show Engine emergency latch and command path are established.
- GPIO25 is the momentary physical emergency trigger; GPIO24 is the emergency NeoPixel line.
- P4 SD/WebUI work exists in the current Stage Controller firmware.
- PCM5102A show-audio wiring is defined at GPIO20/21/22.
- The final hardware baseline now removes separate C3 and DS3231 modules.
- Onboard C6, internal RTC backup and onboard system audio are the next Stage Controller integration tasks.

## Long-term vision

Hardware may evolve; roles stay fixed. Shows are authored and stored against the Show Engine. Operators use the Director and/or browsers. Nodes remain replaceable specialists on the same fabric.
