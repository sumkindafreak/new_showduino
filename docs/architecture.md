# Showduino v1 Architecture

Showduino v1 is built around a modular core system:

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
        │ UART bridge command link
        ▼
┌─────────────────────────────┐
│ Showduino ESP-NOW Bridge    │
│ ESP32-C6 / ESP32-C3 / ESP32 │
└─────────────────────────────┘
        │
        │ ESP-NOW
        ▼
┌─────────────────────────────┐
│ Showduino Relay Node        │
│ ESP32 + relay module board  │
└─────────────────────────────┘
```

## Core decision

The old Arduino Mega executor role is now replaced by the **ESP32-P4 Stage Engine**.

Relays are **not** directly owned by the P4. Relay hardware lives on one or more wireless **ESP-NOW Relay Nodes**.

The product roles are now:

- **Director** — ESP32-S3 touchscreen and WebUI server
- **Stage Engine** — ESP32-P4 deterministic show executor
- **ESP-NOW Bridge** — ESP32-C6/C3/S3 wireless bridge between P4 and ESP-NOW nodes
- **Relay Node** — ESP32 relay module board using ESP-NOW
- **Nodes** — wireless props, puzzles, sensors and effects
- **Studio** — browser editor served from the Director

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

## 2. Showduino Stage Engine

Target hardware:

- ESP32-P4
- UART command link from the Director
- UART command link to the ESP-NOW Bridge
- Local IO for DMX, audio, pixels, inputs, sensors, servos, motor outputs and safety devices

Main responsibilities:

- Execute commands immediately and reliably
- Run timed show sequences
- Route relay commands to ESP-NOW relay nodes
- Handle DMX locally
- Handle NeoPixels locally
- Trigger audio locally
- Read sensors and inputs locally
- Handle servos/motors where fitted
- Enforce emergency stop and safe states
- Keep running even if the Director touchscreen or WebUI crashes

The Stage Engine does **not** need WiFi itself. On the Waveshare ESP32-P4 board, the onboard ESP32-C6 can be used as the ESP-NOW bridge, or a separate ESP32-C3/C6 bridge can be connected by UART.

## 3. ESP-NOW Relay Path

Relay command route:

```text
WebUI / Touchscreen
        ↓
ESP32-S3 Director
        ↓ UART
ESP32-P4 Stage Engine
        ↓ UART
ESP32-C6/C3 ESP-NOW Bridge
        ↓ ESP-NOW
ESP32 Relay Node
        ↓ GPIO
Relay module board
```

A command such as:

```text
RELAY:1:ON
```

means:

1. Director sends `RELAY:1:ON` to Stage Engine.
2. Stage Engine validates safety state.
3. Stage Engine routes the relay command to the ESP-NOW Bridge.
4. Bridge sends a wireless packet to the relay node.
5. Relay node switches relay 1.
6. Relay node replies with status/ACK.
7. Bridge forwards the response back to the Stage Engine.
8. Stage Engine reports status to the Director.

## 4. UART links

### Director to Stage Engine

```text
Director ESP32-S3 TX → Stage Engine ESP32-P4 RX
Director ESP32-S3 RX ← Stage Engine ESP32-P4 TX
GND shared between both boards
```

### Stage Engine to ESP-NOW Bridge

```text
Stage Engine ESP32-P4 TX → Bridge ESP32 RX
Stage Engine ESP32-P4 RX ← Bridge ESP32 TX
GND shared between both boards
```

Default baud rate for both links:

```text
115200
```

## 5. Starter command examples

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

## 6. Capability handshake

On boot, the Director sends:

```text
HELLO
```

The Stage Engine replies with a capability block:

```text
SHOWDUINO_STAGE_ENGINE
FW:0.1.0
RELAYS:ESPNOW
DMX:YES
PIXELS:4
AUDIO:2
INPUTS:16
OUTPUTS:8
SD:YES
READY
```

The Stage Engine can also ask the ESP-NOW Bridge for relay-node status:

```text
BRIDGE:HELLO
```

Bridge replies:

```text
SHOWDUINO_ESPNOW_BRIDGE
FW:0.1.0
MODE:RELAY_ROUTE
READY
```

## 7. Safety model

Emergency stop must override everything.

When the Stage Engine receives:

```text
EMERGENCY:STOP
```

It should:

- Mark the system as emergency locked
- Stop running show timelines
- Stop local dangerous outputs
- Blackout DMX where configured
- Stop dangerous motion outputs
- Forward `EMERGENCY:STOP` to all relay nodes via the ESP-NOW Bridge
- Keep status reporting alive

The Stage Engine only returns to normal after:

```text
EMERGENCY:CLEAR
```

Relay nodes should remain fail-safe until they receive a valid clear command.

## 8. SD card roles

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

## 9. Why this architecture

This split keeps the system reliable and expandable:

- The Director can be rich, visual, editable and networked.
- The Stage Engine can be deterministic, simple and safe.
- Relay boards can be placed where the props are, reducing long relay wiring runs.
- Extra relay nodes can be added without changing the Director/WebUI command language.
- The live show can continue even if the WebUI disconnects.
- Future hardware can change without rewriting the whole platform.

## Current development target

The first concrete build should implement:

1. Director S3 UART bridge scaffold
2. Stage Engine P4 command parser scaffold
3. P4-to-ESP-NOW-Bridge UART relay routing
4. ESP-NOW Bridge scaffold
5. ESP32 Relay Node scaffold
6. HELLO/capability handshake
7. Emergency stop state across Stage Engine and relay nodes
8. Relay command parsing and ACK responses
9. Later: DMX, pixels, audio, SD show playback and timeline engine
