# Showduino AI Agent Guide

This file is a permanent orientation pack for coding agents working in this repository.

## Architecture (as implemented)

Core principle:

> **A running show must not depend on the Director, browser, normal Wi-Fi connectivity or internet access.**

### Component responsibilities

- **Director** (`firmware/director-esp32-8048s050/`): touchscreen operator client that sends requests and displays state.
- **Communications Controller** (`firmware/s3-comms-controller/`): ESP-NOW + UART transport layer, SoftAP host, static WebUI host, API proxy to P4.
- **P4 Show Engine** (`firmware/stage-engine-p4/`): authoritative runtime/timeline/safety/state/storage engine.
- **Specialist nodes** (`firmware/audio-node-esp32-a1s/`, `firmware/s3-lamp-node/`, `firmware/c3-pixel-node/`, others): execute specialist physical/media actions and report status.
- **Studio/WebUI** (`web/showduino-studio/`, embedded into `firmware/s3-comms-controller/.../WebAssets.generated.h`): operator/configuration interface, not runtime authority.

### Control plane vs runtime plane

- **Control plane**: Director UI actions, browser API requests, Studio authoring/commissioning actions, Comms transport/proxying.
- **Runtime plane**: P4 timeline execution, authoritative state machine, emergency latch/safety behavior, local/managed output execution.

## Authority model

- **Runtime state**: P4 Show Engine.
- **Cue execution and timeline**: P4 Show Engine.
- **Emergency/safety state**: P4 Show Engine (physical GPIO25 input + latch), with wireless assert sources routed through architecture.
- **Node commands/orchestration**: P4 decides; Comms transports.
- **Project/show persistent state**: P4 SD storage under `/showduino/...`.
- **Networking transport fabric**: Communications S3 (ESP-NOW/Wi-Fi/AP/STA/UART bridge responsibilities).
- **WebUI hosting**: Communications S3 PROGMEM bundle.
- **Authoritative API/state responses**: P4 (via Comms proxy for browser path).
- **Storage authority**: P4 SD for runtime/show data; Comms/Director local storage is component-local and not show-runtime authority.

## Repository map

- `firmware/stage-engine-p4/` — active Arduino P4 Show Engine firmware.
- `firmware/s3-comms-controller/` — active S3 communications/controller + WebUI host/proxy.
- `firmware/director-esp32-8048s050/` — active touchscreen Director firmware.
- `firmware/audio-node-esp32-a1s/` — specialist Audio Node.
- `firmware/s3-lamp-node/` — specialist Lamp Node.
- `firmware/c3-pixel-node/` — specialist Pixel Node.
- `protocol/` — shared protocol/version/message/validation headers used across components.
- `web/showduino-studio/` — editable Studio/WebUI source.
- `tools/embed-webui/` — WebUI embed/build pipeline for S3 generated assets.
- `stage-engine/esp32-p4/` — separate ESP-IDF P4 stage-engine project/foundation.
- `docs/` — architecture, workflow, pinout, OTA, safety and release documentation.
- `.github/workflows/` — CI workflows (P4 IDF build, WebUI embed workflow).

## Development rules

- Inspect existing implementation before adding/changing behavior.
- Preserve backward compatibility unless explicitly directed otherwise.
- Do not silently change protocol formats, message semantics, or versioning.
- Do not silently change GPIO assignments, board assumptions, or hardware topology.
- Safety and emergency paths always take priority over convenience features.
- Keep runtime authority where architecture defines it (P4 for show/safety/runtime decisions).
- Do not move show-runtime authority into Comms, Director, or browser code.
- Do not duplicate behavior that already exists in another subsystem.
- Do not make running show behavior depend on browser, Director, Wi-Fi association, or internet access.
- Preserve unrelated dirty work; never reset/stash/clean unrelated files.
- Never claim testing that was not actually performed.
- Distinguish **source changed**, **compiled**, **flashed**, and **bench-verified** as separate states.

> **Never redesign a working Showduino subsystem merely because another implementation appears cleaner. First determine why the existing design exists, identify its interfaces and safety assumptions, and make the smallest change that satisfies the requested behaviour.**
