# Showduino v1 Architecture

Showduino v1 is built around a two-board core system:

```text
Phone / Tablet / Laptop
        │
        │ WiFi browser control
        ▼
┌─────────────────────────────┐
│ Showduino Director          │
│ ESP32-S3 + 5 inch display   │
│ Touch UI + WebUI server     │
└─────────────────────────────┘
        │
        │ UART command link
        ▼
┌─────────────────────────────┐
│ Showduino Stage Engine      │
│ ESP32-P4                    │
│ Real-time hardware executor │
└─────────────────────────────┘
        │
        ▼
Relays / DMX / Audio / Pixels / Sensors / Servos / Safety IO
```

## Core decision

The old Arduino Mega executor role is now replaced by the **ESP32-P4 Stage Engine**.

The word `Mega` should only be used historically. The product role is now:

- **Director** — ESP32-S3 touchscreen and WebUI server
- **Stage Engine** — ESP32-P4 hardware executor
- **Nodes** — ESP32-C3/C6/S3 wireless props, puzzles and sensors
- **Studio** — the browser-based editor served from the Director

## 1. Showduino Director

Target hardware:

- ESP32-S3
- 5 inch 800x480 touchscreen
- WiFi AP/STA
- SD card
- UART link to the Stage Engine

Main responsibilities:

- Serve the GoreFX / Showduino Studio WebUI
- Run the local touchscreen interface
- Manage show files and project export/import
- Send commands to the Stage Engine over UART
- Display diagnostics and Stage Engine capability information
- Provide OTA/update features where supported
- Act as the human-facing control surface

The Director should not directly control dangerous show hardware unless specifically required for a small standalone build.

## 2. Showduino Stage Engine

Target hardware:

- ESP32-P4
- No WiFi required
- UART command link from the Director
- Local IO for relays, DMX, audio, pixels, inputs and safety devices

Main responsibilities:

- Execute commands immediately and reliably
- Run timed show sequences
- Handle relays
- Handle DMX
- Handle NeoPixels
- Trigger audio
- Read sensors and inputs
- Handle servos/motors where fitted
- Enforce emergency stop and safe states
- Keep running even if the Director touchscreen or WebUI crashes

## 3. UART link

Primary communication path:

```text
Director ESP32-S3 TX → Stage Engine ESP32-P4 RX
Director ESP32-S3 RX ← Stage Engine ESP32-P4 TX
GND shared between both boards
```

Default baud rate:

```text
115200
```

Starter command examples:

```text
HELLO
STATUS:REQUEST
SHOW:LOAD:ZombieBurst
SHOW:START
SHOW:STOP
RELAY:1:ON
RELAY:1:OFF
RELAY:4:PULSE:2500
AUDIO:1:PLAY:014
DMX:10:255
PIXEL:HELLFIRE
EMERGENCY:STOP
EMERGENCY:CLEAR
```

## 4. Capability handshake

On boot, the Director sends:

```text
HELLO
```

The Stage Engine replies with a capability block:

```text
SHOWDUINO_STAGE_ENGINE
FW:0.1.0
RELAYS:8
DMX:YES
PIXELS:4
AUDIO:2
INPUTS:16
OUTPUTS:8
SD:YES
READY
```

The Director uses this to enable/disable UI features automatically.

## 5. Safety model

Emergency stop must override everything.

When the Stage Engine receives:

```text
EMERGENCY:STOP
```

It should:

- Mark the system as emergency locked
- Stop running show timelines
- Turn off relays that should fail-safe off
- Stop or reduce audio if configured
- Blackout DMX where configured
- Stop dangerous motion outputs
- Keep status reporting alive

The Stage Engine only returns to normal after:

```text
EMERGENCY:CLEAR
```

## 6. SD card roles

### Director SD

Creative/project storage:

```text
/projects/
/shows/
/scenes/
/assets/
/logs/
/settings.json
```

### Stage Engine SD

Runtime/execution storage:

```text
/audio/
/runtime/
/pixels/
/logs/
```

The Director creates and manages shows. The Stage Engine only needs deployed runtime assets.

## 7. Why this architecture

This split keeps the system reliable:

- The Director can be rich, visual, editable and networked.
- The Stage Engine can be deterministic, simple and safe.
- The live show can continue even if the WebUI disconnects.
- Future hardware can change without rewriting the whole platform.

## Current development target

The first concrete build should implement:

1. Director S3 UART bridge scaffold
2. Stage Engine P4 command parser scaffold
3. HELLO/capability handshake
4. Emergency stop state
5. Relay command parsing
6. Status reporting
7. Later: DMX, pixels, audio, SD show playback and timeline engine
