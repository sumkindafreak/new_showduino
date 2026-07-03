# Showduino v1 Architecture

Showduino v1 is split into clear hardware and software layers so each part can be built, tested, and replaced without breaking the whole system.

## Core Philosophy

Showduino should be:

- Modular
- Repairable
- Beginner-serviceable
- Scare attraction focused
- Safe during live operation
- Able to run without internet
- Expandable through add-on nodes
- Usable from touchscreen, web dashboard, and physical triggers

## System Overview

```text
Web Dashboard / GoreFX
        |
        | WiFi / HTTP / WebSocket / local network
        v
Touchscreen Controller ESP32 CYD
        |
        | Serial command protocol
        v
Main Executor Arduino Mega / Future ESP32-S3 Executor
        |
        | Hardware outputs
        v
Relays / DMX / Audio / NeoPixels / Props / Sensors

Optional:

Touchscreen Controller / Web Dashboard
        |
        | ESP-NOW / WiFi
        v
SUE ESP32-S3 Add-on Nodes / R3 Terminals / Wireless Props
```

## Layer 1 — User Interfaces

### CYD Touchscreen Controller

The CYD controller is the physical local interface.

Responsibilities:

- Boot screen
- Main menu
- Show menu
- Diagnostics
- Manual controls
- SD card browser
- Settings
- Add-on status
- Emergency stop trigger
- Send commands to executor
- Receive status from executor

Target folder:

```text
firmware/controller-cyd/
```

### GoreFX Web Dashboard

The GoreFX dashboard is the larger browser-based control system.

Responsibilities:

- Live controls
- Timeline editor
- Show library
- Audio manager
- Diagnostics
- Settings
- Future user/community features

Target folder:

```text
web/gorefx-dashboard/
```

## Layer 2 — Command Bridge

The command bridge translates user actions into stable Showduino commands.

Examples:

```text
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:3000
DMX:1:255
DMX:1-10:0
MP3:A:PLAY:001
PIXEL:1:EFFECT:FIRE
SHOW:LOAD:chamber.shdo
SHOW:PLAY
SHOW:STOP
EMERGENCY:STOP
EMERGENCY:CLEAR
```

Rules:

- Commands should be human-readable.
- Commands should be logged.
- Commands should receive responses where possible.
- Emergency commands must be processed first.
- Old command formats can be supported through compatibility handlers.

## Layer 3 — Main Executor

The executor is the hardware output brain.

Initial target:

- Arduino Mega 2560

Future target:

- ESP32-S3 executor board
- Custom Showduino PCB

Responsibilities:

- Relay outputs
- DMX output
- NeoPixel outputs
- Audio player control
- Digital inputs
- Analog inputs
- Emergency stop
- Status reporting
- Timeline cue execution

Target folder:

```text
firmware/executor-mega/
```

## Layer 4 — Add-on Nodes

Add-on nodes allow Showduino to expand wirelessly or semi-independently.

Types:

- SUE ESP32-S3 node
- R3 terminal puzzle
- Wireless relay node
- Wireless audio node
- Wireless NeoPixel node
- Trigger receiver

Responsibilities:

- Receive ESP-NOW / WiFi commands
- Run local effects
- Report status
- Fail safely if disconnected
- Optionally run standalone

Target folders:

```text
firmware/sue-esp32s3-node/
firmware/addon-nodes/
```

## Layer 5 — Show Files

Showduino v1 should support a human-readable show file format.

Working extension:

```text
.shdo
```

Example concept:

```json
{
  "name": "Chamber Demo",
  "version": 1,
  "cues": [
    {
      "time_ms": 0,
      "command": "RELAY:1:ON"
    },
    {
      "time_ms": 2500,
      "command": "MP3:A:PLAY:001"
    },
    {
      "time_ms": 5000,
      "command": "DMX:1-10:255"
    }
  ]
}
```

The file format should be shared by:

- CYD controller
- GoreFX dashboard
- SD card system
- Future desktop/web tools

## Safety Model

Showduino controls real-world props, relays, lights, audio, and potentially scare hardware. Safety must be built in from the start.

Required safety behaviour:

- Emergency stop turns off all relays.
- Emergency stop blacks out DMX where configured.
- Emergency stop stops show playback.
- Emergency stop pauses or stops audio where configured.
- System should boot with relays off.
- Watchdog/heartbeat should detect communication loss.
- Manual override must be possible.
- Dangerous actions should require deliberate confirmation in UI.

## Hardware Families

### Controller Hardware

- ESP32 CYD / ESP32-2432S028R
- TFT_eSPI
- XPT2046 touch
- SD card
- WiFi
- Serial to executor

### Executor Hardware

- Arduino Mega 2560
- DMX output
- Relay board
- MP3 players
- NeoPixel outputs
- Inputs

### SUE Node Hardware

- ESP32-S3
- FastLED
- RTC
- SD
- PCM5102A audio
- Relay/PWM outputs
- ESP-NOW

## Development Phases

### Phase 0 — Bootstrap

Create docs, structure, audit, and rules.

### Phase 1 — Hardware Baseline

Get CYD + Mega talking reliably.

### Phase 2 — Unified Protocol

Standardise command names and responses.

### Phase 3 — Dashboard Integration

Bring GoreFX dashboard into the v1 structure.

### Phase 4 — Show Playback

Create and run `.shdo` show files.

### Phase 5 — Add-on Expansion

Add SUE, R3, and wireless nodes.

### Phase 6 — Release Candidate

Compile, test, document, tag v1.0.0.
