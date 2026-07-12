# Showduino Development Workflow

## Current working split

Showduino is being developed in parallel so the operator experience and the Stage Engine backend can move forward without blocking each other.

### Director workstream — Toby

The portable 5-inch ESP32-S3 Director is currently being developed separately.

Primary focus:

- LVGL 9 interface
- touchscreen behaviour
- navigation
- visual design
- animations
- operator workflow
- responsive controls

The Director UI should emit clean Showduino commands through one integration point, for example:

```cpp
sendCommand("SHOW:START");
sendCommand("SHOW:STOP");
sendCommand("EMERGENCY:STOP");
sendCommand("LED:TOGGLE");
```

The UI does not need to know whether a command is delivered through ESP-NOW, Ethernet, USB, or a simulator.

When the Director code is ready, it will be reviewed and integrated into this repository without rewriting the finished interface.

### Stage Engine workstream — repository backend

Current repository development focuses on the Waveshare ESP32-P4-Module-DEV-KIT backend:

- ESP32-P4 Stage Engine runtime
- built-in ESP32-C6 wireless subsystem
- supported P4↔C6 SDIO communication path
- Ethernet services
- USB Host services
- SD-card storage
- Showduino Protocol
- command routing
- discovery
- ACK and retry handling
- emergency priority
- local safety behaviour
- logging and diagnostics

## Confirmed Stage Engine hardware

Target board:

**Waveshare ESP32-P4-Module-DEV-KIT**

Confirmed capabilities:

- ESP32-P4 with 32 MB PSRAM
- integrated ESP32-C6
- P4↔C6 SDIO interface
- Wi-Fi 6 and Bluetooth 5 through the C6
- Gigabit RJ45 Ethernet
- optional PoE module interface
- USB 2.0 OTG High Speed
- two USB Type-A ports with Host selection
- SDIO 3.0 microSD slot
- MIPI-DSI display interface
- MIPI-CSI camera interface
- microphone and speaker connections
- RTC battery connection
- 40-pin GPIO expansion header

## Platform roles

### Director

The Director is an operator console. It sends intent, shows status, and can be disconnected without stopping an active show.

### ESP32-C6 communications subsystem

The built-in C6 owns:

- ESP-NOW
- Wi-Fi
- Bluetooth
- portable Director discovery
- wireless packet handling
- communication with the P4 through the supported SDIO link

The exposed C6 UART connector is intended for C6 flashing/debugging and is not the preferred production P4↔C6 data path.

### ESP32-P4 Stage Engine

The P4 owns:

- show runtime
- timeline execution
- authoritative show state
- local emergency handling
- Ethernet
- USB Host
- SD storage
- audio/video services
- device and plugin services

## Immediate integration milestone

Once the finished Director code is shared, the first end-to-end proof is:

```text
Touchscreen button
    ↓
Showduino command
    ↓
ESP-NOW from Director
    ↓
Built-in ESP32-C6
    ↓
Supported SDIO link
    ↓
ESP32-P4 Stage Engine
    ↓
Test GPIO or LED changes state
```

## Rules

1. The Stage Engine is authoritative.
2. An active show must continue if the Director disconnects.
3. The Director UI remains transport-independent.
4. Ethernet is the fixed-show network backbone.
5. ESP-NOW is the portable low-latency Director link.
6. USB Host is a first-class Stage Engine service.
7. Emergency commands receive the highest priority.
8. Board-specific code must be isolated from the shared Showduino protocol and runtime.
9. No unrelated robotics features belong in this project.
