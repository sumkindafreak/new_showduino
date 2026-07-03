# Showduino v1 Migration Plan

This plan turns the scattered Showduino repositories into one clean v1 system.

## Rule One

Do not merge messy code just because it exists.

Every file must earn its place in v1.

## Final Target Repo

Current bootstrap repository:

```text
sumkindafreak/new_showduino
```

Recommended rename:

```text
sumkindafreak/showduino_v1
```

GitHub rename path:

```text
Repository Settings → General → Repository name → showduino_v1 → Rename
```

GitHub will normally redirect old links automatically after the rename.

## Phase 0 — Bootstrap

Status: Started

Tasks:

- Add README
- Add architecture document
- Add source repo audit
- Add migration plan
- Add serial protocol draft
- Add hardware pinout draft
- Add empty firmware/web/docs/hardware structure

Outcome:

A clean planning foundation exists before code is copied.

## Phase 1 — Create Folder Skeleton

Create:

```text
firmware/controller-cyd/
firmware/executor-mega/
firmware/sue-esp32s3-node/
firmware/addon-nodes/r3-terminal/
web/gorefx-dashboard/
docs/
hardware/
examples/
tools/
```

Each firmware target should eventually contain:

```text
README.md
src or Arduino sketch files
config.h
pinout.md
commands.md
```

## Phase 2 — CYD Controller Migration

Source repo:

```text
sumkindafreak/Showduino-controller
```

Target folder:

```text
firmware/controller-cyd/
```

Migration steps:

1. Identify newest working CYD sketch.
2. Extract pin definitions.
3. Remove hard-coded WiFi passwords.
4. Split into modules:
   - display
   - touch
   - menu
   - sd card
   - serial bridge
   - settings
   - diagnostics
5. Compile test.
6. Add upload instructions.
7. Add wiring notes.

Do not migrate until:

- Board target is confirmed.
- Touch pins are confirmed.
- SD pins are confirmed.
- Serial-to-Mega pins are confirmed.

## Phase 3 — Mega Executor Migration

Source repos:

```text
sumkindafreak/arduino_show_controller
sumkindafreak/showduino-complete-code
sumkindafreak/showduino-scare-control-system
```

Target folder:

```text
firmware/executor-mega/
```

Migration steps:

1. Build a clean Mega sketch from scratch.
2. Add relay control first.
3. Add serial command parser.
4. Add emergency stop.
5. Add DMX.
6. Add MP3 control.
7. Add NeoPixel control.
8. Add input handling.
9. Add status reporting.
10. Add timeline cue execution.

Do not migrate until:

- Relay pinout is locked.
- DMX pin is locked.
- MP3 serial pins are locked.
- NeoPixel output pins are locked.
- Emergency stop behaviour is defined.

## Phase 4 — SUE Node Migration

Source repo:

```text
sumkindafreak/SHOWDUINO---SUE
```

Target folder:

```text
firmware/sue-esp32s3-node/
```

Migration steps:

1. Copy modular structure concept.
2. Bring across `config.h` style pin mapping.
3. Bring across LED, RTC, SD, relay, audio, and ESP-NOW modules carefully.
4. Confirm PCM5102A audio library and compile requirements.
5. Add standalone test commands.
6. Add Showduino command bridge compatibility.

Do not migrate until:

- Board variant is confirmed.
- ESP-NOW command direction is defined.
- Audio hardware wiring is confirmed.

## Phase 5 — GoreFX Dashboard Migration

Source repo:

```text
sumkindafreak/gorefx-shadow-control
```

Target folder:

```text
web/gorefx-dashboard/
```

Migration steps:

1. Bring across the Vite/React/TypeScript app.
2. Rename from generic Lovable project to GoreFX / Showduino dashboard.
3. Add API layer for Showduino commands.
4. Add connection settings.
5. Add live controls.
6. Add timeline editor.
7. Add show library.
8. Add diagnostics.
9. Add emergency stop.

Do not migrate until:

- Command protocol is stable enough.
- Dashboard can target a local Showduino IP.
- Emergency stop is always visible.

## Phase 6 — Show File Format

Target:

```text
.shdo
```

Tasks:

- Define show metadata.
- Define cue format.
- Define command format.
- Define timeline timing.
- Define loop/scene support.
- Define emergency behaviour.
- Add examples.

## Phase 7 — Hardware Documentation

Create:

- Master pinout
- CYD pinout
- Mega executor pinout
- SUE pinout
- R3 terminal pinout
- Relay wiring notes
- DMX wiring notes
- Audio wiring notes
- NeoPixel wiring notes
- Power notes

## Phase 8 — First Working v1 Demo

The first v1 demo should be simple and reliable:

1. CYD boots.
2. Mega boots.
3. CYD sends heartbeat.
4. Mega replies OK.
5. CYD button triggers Relay 1.
6. CYD button triggers Relay 1 pulse.
7. Emergency stop turns all relays off.
8. Serial log confirms each command.

Only after this works should we add DMX, audio, pixels, timelines, and web dashboard control.

## Phase 9 — Release Tags

Suggested tags:

```text
v0.1.0-bootstrap-docs
v0.2.0-mega-relay-baseline
v0.3.0-cyd-controller-baseline
v0.4.0-cyd-mega-serial
v0.5.0-dmx-audio-pixels
v0.6.0-gorefx-dashboard
v0.7.0-show-file-playback
v0.8.0-sue-node-support
v0.9.0-release-candidate
v1.0.0-showduino-v1
```

## Success Definition

Showduino v1 is successful when:

- A new user can clone the repo.
- They can upload the Mega firmware.
- They can upload the CYD firmware.
- They can wire relays safely.
- They can trigger effects from touchscreen.
- They can trigger effects from dashboard.
- They can load a show from SD.
- They can stop everything safely.
- The docs explain the full setup clearly.
