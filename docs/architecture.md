# Showduino Architecture

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

### Naming

| Term | Meaning |
|------|---------|
| **Show Engine** | Single source of truth for show state and show services |
| **Stage Controller** | Physical ESP32-P4 product that runs the Show Engine |
| **Communications Engine** | Wireless transport role; target hardware is the Stage Controller's onboard ESP32-C6 |
| **Director** | ESP32-S3 touchscreen operator desk |
| **SUE** | Historical name for communications functionality; no longer a required separate board |
| ~~Stage Engine~~ | Retired term |

Firmware path `firmware/stage-engine-p4/` is the current Show Engine sketch location and retains its old folder name temporarily.

## Non-negotiable ownership

- The Show Engine owns authoritative show state, safety policy, project/configuration storage, Web UI, Web API and WebSocket state.
- The Communications Engine owns wireless transport. It must not make show-level decisions, run the timeline or invent output state.
- The Director commands and displays. It does not host the authoritative show state.
- Nodes act. Application addressing uses logical Showduino device IDs, not raw MAC addresses at the application layer.
- Relay requests use absolute ON/OFF or timed-pulse states, not distributed `TOGGLE`.
- Command acceptance and physical action completion are separate events.
- A running show must not depend on an active Director, browser session, Wi-Fi client association or internet connection.

---

## Hardware baseline — 25 August 2026

The canonical Stage Controller is the **Waveshare ESP32-P4-Module-DEV-KIT**.

```text
┌──────────────────────────┐
│ Director ESP32-S3        │
│ commands + display       │
└────────────┬─────────────┘
             │ ESP-NOW
             ▼
┌──────────────────────────┐
│ onboard ESP32-C6         │
│ Communications Engine    │
│ ESP-NOW / Wi-Fi / BT     │
└────────────┬─────────────┘
             │ integrated P4/C6 transport
             │ target: board SDIO path
             ▼
┌──────────────────────────┐
│ ESP32-P4 Show Engine     │
│ Stage Controller         │
│ decides + stores + serves│
└────────────┬─────────────┘
             │
             ├── local emergency / pixels
             ├── onboard system audio
             ├── external PCM show audio
             └── node commands via C6 fabric
```

The P4 board also supplies the target RTC, microSD, Ethernet, USB, microphone and speaker-amplifier hardware.

A separate external C3/SUE board and separate DS3231 RTC are no longer part of the final Stage Controller hardware baseline.

See [`hardware-baseline-2026-08-25.md`](hardware-baseline-2026-08-25.md).

---

## Firmware transition boundary

The physical hardware decision is ahead of the communications firmware.

### Final target

```text
Director → ESP-NOW → onboard C6 → integrated P4/C6 transport → P4 Show Engine
Node     → ESP-NOW → onboard C6 → integrated P4/C6 transport → P4 Show Engine
Browser  → Wi-Fi   → onboard C6 → P4 Web services
```

The Waveshare module integrates the ESP32-C6 with the P4 using **SDIO** for Wi-Fi/Bluetooth expansion. The C6 also exposes UART pins for flashing/debugging.

### Previous working compatibility path

```text
Director → ESP-NOW → external C3 SuperMini → UART → P4
Node     → ESP-NOW → external C3 SuperMini → UART → P4
Browser  → Wi-Fi   → external C3 SuperMini → UART tunnel → P4
```

That external C3 firmware remains in the repository as a known-good migration reference and rollback path. It is not the final physical design.

The onboard C6 must not be called production-ready until it has parity for:

- Director receive and reply traffic
- Show Engine state/ACK delivery back to the Director
- Node forwarding and node replies
- WebUI AP/STA transport
- Emergency traffic
- Link health and recovery
- No false-success routes

---

## Role details

### 1. Director — ESP32-S3

**Active firmware:** `firmware/director-esp32-8048s050/`

Responsibilities:

- Operator touch UI (LVGL)
- Emit requests: show control, cues, outputs, emergency
- Display authoritative Show Engine state
- Diagnostics for link and local hardware

Not responsibilities:

- Timeline execution
- Direct control of Stage Controller or node GPIOs
- Final project authority
- Global safety policy

### 2. Communications Engine — onboard ESP32-C6

**Target firmware:** `firmware/p4-c6-espnow-bridge/`

Responsibilities:

- ESP-NOW desk link to Director
- ESP-NOW fabric to nodes
- Wi-Fi AP/STA for browsers
- Transport between radio side and Show Engine
- Link health / transport-address resolution

Must not:

- Run shows
- Own show state
- Control DMX/pixels/audio as an authority
- Return false success for unimplemented routes

The existing onboard-C6 sketch is **bring-up code**, not final transport firmware. Its old placeholder UART assumptions must not be treated as the final internal P4↔C6 architecture.

### 3. Show Engine — ESP32-P4 Stage Controller

**Active firmware:** `firmware/stage-engine-p4/`

Responsibilities:

- Authoritative show/runtime/emergency state
- Timeline and cue scheduling
- Project storage and configuration
- Request validation and state publication
- Node coordination by logical device ID
- Local Stage Controller outputs
- Web UI / Web API / WebSocket
- System time ownership

### 4. Nodes

Nodes execute specialist physical work and report results. Examples: relay, lighting, audio zone, sensor, motor and environmental nodes.

---

## RTC / system time

The Stage Controller uses the **ESP32-P4 RTC domain** and the Waveshare board's rechargeable RTC backup-battery connection as the target timekeeping hardware.

The external DS3231 is removed from the final hardware baseline.

Important maturity note: battery-backed RTC behaviour must be qualified in the actual Showduino firmware/board state. Do not claim power-loss retention until tested.

The Show Engine is the authoritative time source presented to Director/WebUI clients.

---

## Audio architecture

Audio is split by responsibility.

### Showduino/system audio

```text
ESP32-P4
  → onboard ES8311 codec
  → onboard NS4150B amplifier
  → 8Ω / 2W speaker header
```

Reserved for short system sounds: boot, ready, loaded, armed, warning/error, emergency acknowledgement and restart/shutdown cues.

### Show/programme audio

```text
ESP32-P4
  → external PCM5102A
  → show audio amplification / playback system
```

Reserved for timeline content: music, dialogue, ambience and timed SFX.

The P4 provides one I2S peripheral, so v1 firmware must arbitrate the audio resource. The architecture does not promise independent simultaneous playback on both output paths until that is deliberately implemented and tested.

---

## Stage Controller local resource map

Canonical assignments live in `docs/hardware-pinout.md` and the Stage Controller `BoardConfig.h`.

Current high-level assignments:

```text
GPIO7-13   onboard ES8311 control/audio
GPIO20-22  external PCM5102A show audio
GPIO24     emergency NeoPixel data
GPIO25     momentary emergency button
GPIO39-45  onboard microSD / SDMMC + power
GPIO53     onboard audio amplifier enable
```

---

## Safety model

Emergency stop overrides entertainment commands.

When the Show Engine accepts emergency activation it must:

- Enter emergency-latched state
- Stop/freeze show execution as appropriate
- Command dangerous outputs toward safe states
- Keep status reporting available
- Require explicit clear/reset policy before normal operation

Current physical emergency input remains a momentary button on **GPIO25**. Pressing it latches emergency in software. Releasing or pressing again does not clear it. A clear is rejected while the button remains physically asserted.

The emergency NeoPixel line remains on **GPIO24**.

---

## Storage

| Store | Owner |
|-------|-------|
| Shows, projects, configuration | Show Engine / P4 SD |
| Runtime media | Show Engine / P4 SD and specialist nodes where required |
| Director UI assets | Director-local storage |
| WebUI runtime assets | P4 SD under `/showduino/webui/` |

---

## Why this architecture

- One authoritative show engine
- Fewer separate modules and cables inside the Stage Controller
- Uses the P4 board's integrated radio, RTC, audio, storage, Ethernet and USB capabilities
- Wireless failures degrade transport, not necessarily local show execution
- Director and WebUI remain clients of the same truth model
- Nodes remain replaceable specialists

---

## Development status

**Hardware baseline locked:** P4 + onboard C6 + P4 RTC + onboard system audio + external PCM show audio.

**Already working in current P4 code:** command/state path, emergency latch, SD/WebUI pieces, emergency GPIO24/25 and external PCM pin definitions.

**Migration work still required:** onboard C6 production transport, P4 RTC backup qualification, onboard ES8311 system-sound driver and I2S arbitration between system/show audio.
