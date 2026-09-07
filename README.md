# Showduino v1

**Showduino** is a modular, distributed show-control platform for scare attractions, escape rooms, immersive experiences, and interactive props.

## Architectural constitution

> The Show Engine decides.  
> The Communications Engine transports.  
> The Director commands and displays.  
> The Nodes act.

- The **Show Engine** on the ESP32-P4 is the single source of truth for show state, safety policy, production runtime, configuration and authoritative device state.
- The **Communications Engine** is the dedicated ESP32-S3. It transports ESP-NOW/UART traffic and currently hosts the static bench/browser WebUI plus API proxy. It must not make show-level decisions.
- The **Director** is the ESP32-S3 touchscreen operator interface. Commands are requests; authoritative state comes back from the P4.
- A running show must **not** depend on an active Director, browser, Wi-Fi client, or internet connection.
- Application code should address devices by logical Showduino device IDs; MAC addresses remain transport details.
- Command acceptance and physical completion are separate lifecycle events.

The Waveshare onboard ESP32-C6 remains **unused/reserved**.

## Current live topology

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
Dedicated ESP32-S3 Communications Engine
        │ UART 115200 8N1
        ▼
ESP32-P4 Show Engine / Stage Controller
        │
        ├── local system/safety audio (ES8311)
        ├── GPIO24 emergency/designated-signage NeoPixels
        ├── GPIO23 segmented theatrical Show Pixel Line
        └── authoritative production/runtime state

Specialist Nodes
        │ ESP-NOW through Communications Engine
        ▼
P4 authoritative control
```

## Browser / phone path

The Communications S3 now hosts the static Showduino browser UI from PROGMEM and proxies P4 API requests:

```text
Phone / Tablet / Laptop
        │ Wi-Fi / Showduino SoftAP
        ▼
S3 Communications Engine — static WebUI / transport proxy
        │ UART
        ▼
P4 Show Engine — authoritative state/API data
```

The S3 does not become the Show Engine merely because it serves the HTML/JS.

## Core roles

### Show Engine — ESP32-P4 Stage Controller

Current implemented foundation includes:

- authoritative runtime/emergency state;
- start/pause/resume/stop timeline runtime;
- transactional loading of versioned TEST/LOG productions from P4 SD;
- P4 onboard ES8311 system/safety audio;
- Audio Node routing/state tracking;
- GPIO24 emergency/designated-signage pixel engine;
- GPIO23 segmented local Show Pixel Engine;
- local storage, Plug-in Bus, network and diagnostics foundations.

Persistent production format v1 still accepts only TEST/LOG cues. `AUDIO` and `PIXEL` production cue parsing/dispatch remain follow-up work.

### Communications Engine — dedicated ESP32-S3

Current responsibilities:

- ESP-NOW with Director and specialist nodes;
- UART with P4 (`P4 RX=GPIO4`, `P4 TX=GPIO5`; S3 TX=GPIO17, RX=GPIO18);
- Audio Node packet forwarding;
- transport/link health;
- SoftAP and static WebUI hosting;
- proxying browser API requests to authoritative P4 services.

It must not run timelines or invent show state.

### Director — ESP32-S3 touchscreen

- show selection and run-control requests;
- node/output controls;
- emergency workflow;
- authoritative-state display;
- local UI/diagnostic behavior.

### Specialist Nodes

Current rollout order is intentionally fixed:

```text
1. Audio Node
2. C3 Lantern Node
3. C3 Pixel Node
4. MOSFET Node
```

The old Relay Node product concept is superseded by the MOSFET Node direction. Legacy relay source remains for reference only.

DMX remains **parked/out of scope** until explicitly reopened.

## P4 pixel architecture

### GPIO24 — emergency/designated-signage line

One emergency exit sign is a 10-pixel bundle. The configured maximum is 100 pixels / ten signs.

Normal state:

```text
pixel 0 GREEN, 1-9 OFF
pixel 10 GREEN, 11-19 OFF
pixel 20 GREEN, 21-29 OFF
...
```

Emergency state:

```text
ALL GPIO24 PIXELS BRIGHT WHITE
```

All groups are prepared in one frame so the signs switch together.

### GPIO23 — local Show Pixel Line

The P4 now contains a non-blocking segmented FX engine with up to 16 configured segment slots and the shared Showduino 25-effect vocabulary.

Example simultaneous use:

```text
pixels 0-7    LIGHTNING
pixels 8-10   SOLID BLUE
pixels 11-29  FIRE
pixels 30-49  PULSE RED
pixels 50-79  FLICKER WARM WHITE
```

Hard Showduino emergency rule:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

This applies to GPIO23, GPIO24, and every future pixel-capable node. Clearing emergency returns GPIO24 to green locator markers and leaves normal show pixels blacked out; interrupted FX do not auto-resume.

Shared effect vocabulary: [`protocol/showduino_pixel_fx.h`](protocol/showduino_pixel_fx.h).

## Pixel electrical standard

Each P4 pixel data output uses a **470 Ω series resistor** near the controller/logic buffer:

```text
GPIO23 / 5V buffer → 470 Ω → Show Pixel DIN
GPIO24 / 5V buffer → 470 Ω → Emergency/Signage DIN
```

Use common P4/pixel ground. Final/long-cable installations should use a 5 V-compatible logic buffer such as a 74AHCT125/74HCT125-class device. The 470 Ω resistor is not a level shifter. Pixels should use a suitably sized external 5 V supply, with bulk capacitance and power injection appropriate to the installation.

## Firmware map

### Active / current

```text
firmware/director-esp32-8048s050/     Director
firmware/s3-comms-controller/         Communications Engine
firmware/stage-engine-p4/             Show Engine / Stage Controller
firmware/audio-node-esp32-a1s/        Audio Node — hardware test required
```

### Planned

```text
C3 Lantern Node                        next specialist-node milestone
C3 Pixel Node                          follows Lantern; shares common FX vocabulary
firmware/mosfet-node-esp32/           planned; replaces Relay Node concept
```

### Legacy / reserved / diagnostic

```text
firmware/relay-node-esp32/            LEGACY / SUPERSEDED product direction
firmware/p4-c6-espnow-bridge/         UNUSED / RESERVED onboard C6 work
firmware/c3-supermini-espnow-bridge/  LEGACY / SUPERSEDED SUE Comms
firmware/director-s3/                 LEGACY
firmware/espnow-bridge/               LEGACY
firmware/touch-probe-8048/            DIAGNOSTIC
firmware/controller-cyd/              ARCHIVE CANDIDATE
firmware/executor-mega/               ARCHIVE CANDIDATE
```

## Documentation

| Document | Contents |
|----------|----------|
| [`docs/constitution.md`](docs/constitution.md) | Permanent architectural rules |
| [`docs/architecture.md`](docs/architecture.md) | System architecture and maturity |
| [`docs/repository-status.md`](docs/repository-status.md) | Current firmware classification |
| [`docs/final-hardware-architecture.md`](docs/final-hardware-architecture.md) | Hardware topology |
| [`docs/hardware-pinout.md`](docs/hardware-pinout.md) | Current P4 pins and pixel wiring standard |
| [`docs/audio-pixel-engine.md`](docs/audio-pixel-engine.md) | Audio split, segmented FX and emergency-pixel policy |
| [`docs/audio-node.md`](docs/audio-node.md) | ESP32-A1S / ES8388 Audio Node |
| [`docs/node-roadmap.md`](docs/node-roadmap.md) | Audio → Lantern → C3 Pixel → MOSFET rollout |
| [`docs/production-storage.md`](docs/production-storage.md) | Persistent P4 production format |
| [`docs/plugin-bus.md`](docs/plugin-bus.md) | Showduino Plug-in Bus |

## Current milestone

The immediate platform target is to bench-commission the **P4 pixel lines**:

1. prove GPIO24 normal green-sign pattern;
2. prove synchronized full-white emergency signage;
3. prove GPIO23 direct colour and commissioning test;
4. prove multiple simultaneous segmented FX;
5. prove emergency forces GPIO23 + GPIO24 white;
6. prove clear returns signage to locator green and show pixels to safe blackout.

After current P4/Audio work, the next specialist node is the **C3 Lantern Node**, followed by **C3 Pixel**, then **MOSFET**.
