# Showduino v1

**Showduino v1** is the first clean, unified build of the Showduino scare attraction control system.

This repository is intended to bring together the best parts of the previous Showduino, GoreFX, SUE, controller, and Arduino show control experiments into one stable product-style codebase.

> Working name: `showduino_v1`

## Project Goal

Create one complete, documented, modular control platform for scare attractions, immersive rooms, props, lighting, audio, DMX, relays, sensors, timelines, and touchscreen/web control.

## Core System Pieces

### 1. Touchscreen Controller

Target hardware:

- ESP32 CYD / ESP32-2432S028R style touchscreen
- TFT display
- XPT2046 touch
- SD card
- WiFi AP / STA
- Serial connection to Mega or executor board

Purpose:

- Local control panel
- Show selection
- Diagnostics
- Manual trigger controls
- Settings
- SD show file management
- Status display

Source references:

- `sumkindafreak/Showduino-controller`
- `sumkindafreak/showduino-complete-code`

### 2. Main Executor Firmware

Target hardware:

- Arduino Mega 2560 initially
- Future ESP32-S3 executor option

Purpose:

- Relays
- DMX
- MP3 / audio players
- NeoPixel outputs
- Sensors / inputs
- Emergency stop
- Timeline command execution

Source references:

- `sumkindafreak/arduino_show_controller`
- `sumkindafreak/showduino-complete-code`
- `sumkindafreak/showduino-scare-control-system`

### 3. SUE / ESP32-S3 Add-on Node

Target hardware:

- ESP32-S3 XH3SE / SUE-style board
- FastLED output
- RTC
- SD card
- relay outputs
- PCM5102A I2S audio
- ESP-NOW communication

Purpose:

- Wireless prop node
- Audio node
- Relay node
- Scare effect node
- Future expansion node

Source references:

- `sumkindafreak/SHOWDUINO---SUE`

### 4. GoreFX Dashboard

Target platform:

- Vite
- React
- TypeScript
- Tailwind CSS
- shadcn/ui

Purpose:

- Browser dashboard
- Live controls
- Timeline editor
- Show library
- Audio manager
- Settings
- Diagnostics
- Future community/show sharing

Source references:

- `sumkindafreak/gorefx-shadow-control`
- `sumkindafreak/showduino-fx-control`

## Planned Repository Structure

```text
showduino_v1/
├── firmware/
│   ├── controller-cyd/
│   ├── executor-mega/
│   ├── sue-esp32s3-node/
│   └── addon-nodes/
├── web/
│   └── gorefx-dashboard/
├── docs/
│   ├── architecture.md
│   ├── migration-plan.md
│   ├── source-repo-audit.md
│   ├── serial-protocol.md
│   ├── hardware-pinout.md
│   └── setup-guide.md
├── hardware/
│   ├── wiring/
│   ├── pcb-notes/
│   └── bom.md
├── examples/
│   ├── chamber-demo/
│   ├── relay-test/
│   ├── dmx-test/
│   └── audio-test/
└── tools/
    └── show-file-tools/
```

## Development Rules

1. Every firmware build must compile independently.
2. All pins must be defined clearly at the top of each firmware project.
3. Every hardware target gets its own folder.
4. No secret credentials are committed.
5. WiFi passwords must live in local config files or be entered through setup UI.
6. Serial commands must be documented before being added.
7. Safety actions such as emergency stop must override everything.
8. Stable builds are tagged as releases.

## Current Status

Phase 0 has started.

The first task is to audit all existing repositories, identify the best source files, and then migrate code into this clean structure.

## Previous Source Repositories

- https://github.com/sumkindafreak/SHOWDUINO---SUE
- https://github.com/sumkindafreak/new_showduino
- https://github.com/sumkindafreak/Showduino-controller
- https://github.com/sumkindafreak/showduino-complete-code
- https://github.com/sumkindafreak/gorefx-shadow-control
- https://github.com/sumkindafreak/showduino-fx-control
- https://github.com/sumkindafreak/showduino-scare-control-system
- https://github.com/sumkindafreak/arduino_show_controller

## Phase Roadmap

### Phase 0 — Repo Bootstrap

- Create clean structure
- Add source audit
- Add architecture plan
- Add serial protocol draft
- Add hardware pinout draft

### Phase 1 — Stable Hardware Baseline

- Mega executor firmware
- CYD controller firmware
- Basic serial protocol
- Relay test
- DMX test
- Audio test
- NeoPixel test

### Phase 2 — Web Dashboard

- Bring in GoreFX dashboard
- Connect dashboard commands to controller/executor
- Add live controls
- Add timeline editor foundation

### Phase 3 — Show Files

- Define `.shdo` show format
- Add SD card show loading
- Add timeline playback
- Add import/export tools

### Phase 4 — Add-on Nodes

- SUE ESP32-S3 node
- ESP-NOW command bridge
- R3 terminal style add-ons
- Wireless trigger support

### Phase 5 — Product Polish

- Setup guide
- Wiring diagrams
- BOM
- Release builds
- Example attraction projects

---

Showduino v1 begins here.
