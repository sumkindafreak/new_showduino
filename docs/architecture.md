# Showduino v1 Architecture

Showduino v1 has one controller and one executor.

This document corrects and locks the first real v1 architecture so the project stays clear and buildable.

## v1 Hardware Rule

For Showduino v1:

```text
There is only ONE controller: ESP32 CYD touchscreen.
There is only ONE executor: Arduino Mega 2560.
All show hardware connects to the Mega.
No extra ESP32 nodes are part of v1.
```

Extra ESP32, SUE, ESP-NOW, R3 terminals, wireless nodes, and distributed audio nodes are future expansion ideas only.

They are not part of the first v1 build.

## Core Philosophy

Showduino v1 should be:

- Clear
- Buildable
- Repairable
- Beginner-serviceable
- Scare attraction focused
- Safe during live operation
- Able to run without internet
- Controlled locally from the CYD touchscreen
- Designed around scene creation, pixels, and audio

## System Overview

```text
GoreFX Dashboard / Scene Creator
        |
        | WiFi / local network to CYD
        v
ESP32 CYD Touchscreen Controller
        |
        | Serial command protocol
        v
Arduino Mega 2560 Executor
        |
        | Direct hardware wiring
        v
Relays / DMX / Pixels / Audio Board / Sensors / Props
```

## What Each Board Does

## 1. CYD Touchscreen Controller

The CYD is the user interface and command controller.

Responsibilities:

- Boot screen
- Main menu
- Show menu
- Manual controls
- Scene list
- Diagnostics
- Settings
- Emergency stop button
- SD card browsing if fitted/used
- WiFi access point or local network connection
- Receive commands from GoreFX dashboard
- Send commands to the Mega executor
- Receive status from Mega

The CYD should not directly run the show hardware.

The CYD tells the Mega what to do.

Target folder:

```text
firmware/controller-cyd/
```

## 2. Arduino Mega Executor

The Mega is the hardware brain.

All live show hardware connects here.

Responsibilities:

- Relay outputs
- DMX output
- NeoPixel outputs
- PCM/local audio board control if connected to Mega
- Digital inputs
- Analog inputs
- Sensors
- Props
- Emergency stop behaviour
- Status reporting
- Scene playback commands
- Timeline cue execution where needed

Target folder:

```text
firmware/executor-mega/
```

## 3. GoreFX Dashboard / Scene Creator

The dashboard is the main scene creation interface.

Responsibilities:

- Create scenes
- Edit timeline cues
- Build pixel effects
- Add audio cues
- Add relay cues
- Add DMX cues
- Save `.shdo` scene files
- Send commands to CYD
- Preview/test cues where possible

The dashboard talks to the CYD, not directly to random hardware.

Target folder:

```text
web/gorefx-dashboard/
```

## Hardware Output Direction

All hardware in v1 should be treated as Mega-connected hardware.

```text
Mega outputs:
- 8 relays
- DMX
- NeoPixel data lines
- Audio trigger/control lines or serial/audio board control
- Sensor inputs
- Prop outputs
```

## Audio Direction for v1

The desired audio direction is local SD-stored audio using a PCM audio board.

Important v1 clarification:

```text
The PCM/audio hardware is part of the Mega executor hardware system.
There is no separate ESP32 audio node in v1.
```

The exact audio board wiring and control method still needs to be confirmed.

Possible Mega audio approaches:

1. Mega triggers a dedicated SD/PCM audio playback board.
2. Mega controls an audio playback module over serial/SPI/I2C if supported.
3. CYD manages files/UI while Mega handles playback commands.

The v1 design must not assume an extra ESP32 exists.

## Pixel Direction for v1

Pixels connect to the Mega.

The Mega should run the pixel engine for v1.

Required pixel features:

- Multiple pixel outputs if memory allows
- Sub-strip support
- Scene cue support
- Brightness
- Colour
- Speed
- Duration
- Non-blocking effects

## Scene Creator Direction

The Scene Creator remains the heart of Showduino.

But the execution path is:

```text
GoreFX Scene Creator
        ↓
CYD Controller
        ↓
Mega Executor
        ↓
Pixels / Audio / Relays / DMX
```

## Command Bridge

The command bridge translates user actions into stable Showduino commands.

Preferred v1 style:

```text
CATEGORY:TARGET:ACTION:VALUE
```

Examples:

```text
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:3000
DMX:1:255
DMX:1-10:0
AUDIO:PLAY:001
PIXEL:1:EFFECT:FIRE
SCENE:LOAD:chamber_intro.shdo
SCENE:PLAY
SCENE:STOP
EMERGENCY:STOP
EMERGENCY:CLEAR
```

Rules:

- Commands should be human-readable.
- Commands should be logged.
- Commands should receive responses where possible.
- Emergency commands must be processed first.
- Old command formats can be supported through compatibility handlers.

## Show Files

Showduino v1 should support a human-readable scene/show file format.

Working extension:

```text
.shdo
```

Example concept:

```json
{
  "name": "Chamber Demo",
  "version": 1,
  "duration_ms": 10000,
  "cues": [
    {
      "time_ms": 0,
      "type": "AUDIO",
      "command": "AUDIO:PLAY:001"
    },
    {
      "time_ms": 0,
      "type": "PIXEL",
      "command": "PIXEL:1:EFFECT:PULSE:255,0,0"
    },
    {
      "time_ms": 7000,
      "type": "RELAY",
      "command": "RELAY:1:PULSE:1000"
    }
  ]
}
```

## Safety Model

Showduino controls real-world props, relays, lights, audio, and potentially scare hardware. Safety must be built in from the start.

Required safety behaviour:

- Emergency stop turns off all relays.
- Emergency stop blacks out DMX where configured.
- Emergency stop stops scene playback.
- Emergency stop stops audio where configured.
- Emergency stop blacks out/stops pixels where configured.
- System must boot with relays off.
- Mega must handle hardware safety even if CYD disconnects.
- Watchdog/heartbeat should detect communication loss.
- Manual override must be possible.
- Dangerous actions should require deliberate confirmation in UI.

## v1 Development Phases

### Phase 0 — Bootstrap

Create docs, structure, audit, and rules.

### Phase 1 — CYD + Mega Communication

Get CYD and Mega talking reliably over serial.

Required demo:

```text
CYD sends HEARTBEAT
Mega replies STATUS:ALIVE
CYD sends RELAY:1:ON
Mega turns relay 1 on
CYD sends EMERGENCY:STOP
Mega makes all outputs safe
```

### Phase 2 — Mega Pixel Engine

Add non-blocking pixel effects on the Mega.

### Phase 3 — Mega Audio Control

Add local audio board control through the Mega.

### Phase 4 — Scene Playback

Run a timed scene using audio, pixels, and relay cues.

### Phase 5 — GoreFX Scene Creator

Build scene files visually and send/run them through CYD/Mega.

### Phase 6 — DMX and Advanced Outputs

Add DMX cues and advanced hardware features.

### Phase 7 — Release Candidate

Compile, test, document, and tag v1.0.0.

## Future Expansion, Not v1

These ideas are parked for later:

- SUE ESP32-S3 nodes
- ESP-NOW wireless add-ons
- R3 terminal nodes
- Separate audio ESP32
- Distributed pixel nodes
- Wireless relay nodes

They are useful future ideas, but they must not confuse the first v1 build.

## Final v1 Design Statement

Showduino v1 is:

```text
One CYD controller.
One Mega executor.
All hardware connected to the Mega.
Scene creation focused on pixels, audio, and timing.
```
