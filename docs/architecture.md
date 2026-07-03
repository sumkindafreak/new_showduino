# Showduino v1 Architecture

Showduino v1 has one controller and one executor.

This document locks the weekend test architecture so the project stays clear and buildable.

## v1 Hardware Rule

For Showduino v1:

```text
Controller: ESP32 CYD touchscreen
Executor:   Arduino Mega 2560
Hardware on Mega:
- 4 pixel output lines
- SD card reader
- RTC module
- Audio control layer for local SD/PCM-style audio hardware

Not onboard for first v1:
- Relay bank
- DMX
- Extra ESP32 executor
```

Relay output is planned later as a separate ESP-NOW expansion node system.

## System Overview

```text
GoreFX Dashboard / Scene Creator
        |
        | WiFi / local network
        v
ESP32 CYD Touchscreen Controller
        |
        | UART serial command protocol
        v
Arduino Mega 2560 Executor
        |
        | Direct hardware wiring
        v
4 Pixel Lines / SD Card / RTC / Audio Control Hardware

Future expansion:
        |
        v
ESP-NOW Relay Nodes / Wireless Props / Add-ons
```

## CYD Controller Role

The CYD is the user interface and command controller.

Responsibilities:

- Boot screen
- Main menu
- Scene menu
- Manual controls
- Diagnostics
- Settings
- Emergency stop button
- WiFi AP/local network connection
- Receive commands from GoreFX dashboard
- Send commands to Mega
- Receive status from Mega

The CYD does not run the show hardware directly.

Target folder:

```text
firmware/controller-cyd/
```

## Mega Executor Role

The Mega is the v1 hardware brain.

Core hardware connected to the Mega:

- Pixel line 1
- Pixel line 2
- Pixel line 3
- Pixel line 4
- SD card reader
- RTC module
- Audio control/trigger interface

Responsibilities:

- Run non-blocking pixel engine
- Read/check SD card
- Use RTC for time/date/status
- Control or trigger local audio hardware
- Receive commands from CYD
- Report status to CYD
- Run scene test timing
- Handle emergency stop / blackout behaviour

Target folder:

```text
firmware/executor-mega/
```

## Important Audio Note

A bare PCM5102A board normally expects I2S audio data.

Arduino Mega does not provide native ESP32-style I2S audio output, so the first weekend build treats audio as an abstract control layer until the exact audio board/control method is confirmed.

The design goal remains:

```text
Local SD-stored audio, triggered in sync with pixel scenes.
```

For testing, the Mega firmware will expose commands like:

```text
AUDIO:PLAY:001
AUDIO:STOP
AUDIO:VOLUME:80
```

These commands can later be mapped to the chosen audio board interface.

## Pixel Direction for v1

Pixels connect to the Mega.

Required pixel features:

- 4 output lines
- Independent line control
- Solid colour
- Pulse
- Fire/flicker-style effect
- Strobe
- Blackout
- Brightness
- Speed
- Duration
- Non-blocking updates

## Scene Creator Direction

The Scene Creator is the heart of Showduino.

Execution path:

```text
GoreFX Scene Creator
        ↓
CYD Controller
        ↓
Mega Executor
        ↓
Pixels / Audio / SD / RTC
```

## Weekend Hardware Test Target

The first real bench test should prove:

```text
1. Mega boots.
2. RTC starts or reports missing.
3. SD starts or reports missing.
4. Four pixel lines initialise.
5. CYD sends HEARTBEAT.
6. Mega replies STATUS:ALIVE.
7. CYD sends PIXEL commands.
8. Mega runs pixel effects.
9. CYD sends AUDIO commands.
10. Mega logs/triggers audio abstraction.
11. CYD sends SCENE:TEST.
12. Mega plays a timed pixel/audio test scene.
13. CYD sends EMERGENCY:STOP.
14. Mega blackouts pixels and stops scene/audio.
```

## Core Commands

```text
HEARTBEAT
STATUS:REQUEST
PIXEL:ALL:BLACKOUT
PIXEL:1:COLOR:255,0,0
PIXEL:1:EFFECT:PULSE
PIXEL:1:EFFECT:FIRE
PIXEL:1:EFFECT:STROBE
AUDIO:PLAY:001
AUDIO:STOP
AUDIO:VOLUME:80
RTC:STATUS
SD:STATUS
SCENE:TEST
SCENE:STOP
EMERGENCY:STOP
EMERGENCY:CLEAR
```

## Future Expansion, Not Weekend v1

Parked for later:

- ESP-NOW relay nodes
- DMX
- SUE ESP32-S3 nodes
- R3 terminal nodes
- Wireless audio nodes
- Wireless pixel nodes

## Final v1 Design Statement

```text
One CYD controller.
One Mega executor.
Mega handles 4 pixel lines, SD, RTC, and audio control.
Scene creation focuses on pixels, audio, and timing.
Relays become a later ESP-NOW expansion system.
```
