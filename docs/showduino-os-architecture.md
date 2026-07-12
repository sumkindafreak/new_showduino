# Showduino OS Architecture

> Showduino is the show.

Showduino is an operating system for creating, running, and managing live immersive experiences.

It is not defined by one board, one touchscreen, or one protocol. The hardware exists to serve the platform.

## Core topology

```text
Portable Director
ESP32-S3 + 5in Touch
        |
     ESP-NOW
        |
Built-in ESP32-C6
Wireless bridge on P4 board
        |
  Internal board link
        |
ESP32-P4 Stage Engine
        |
  +-----+------------------+
  |                        |
Ethernet                 USB Host
  |                        |
Art-Net / sACN / OSC      Storage / MIDI / Audio
Other Stage Engines       Keyboards / Control surfaces
PCs / WebUI               Import / Backup / Interfaces
```

## Director

The Director is the portable operator console.

Responsibilities:

- Live show control
- Scene selection
- Emergency control
- Diagnostics
- Show deployment
- Status monitoring
- Operator feedback

The Director is not authoritative. If it disconnects, the Stage Engine continues running safely.

## ESP32-C6 bridge

The built-in C6 provides the wireless gateway to the P4 Stage Engine.

Responsibilities:

- ESP-NOW receive/transmit
- Discovery
- ACK/retry handling
- Wireless provisioning
- Forwarding messages to the P4
- Future Wi-Fi services where appropriate

The C6 is a transport processor, not the show engine.

## ESP32-P4 Stage Engine

The P4 is the wired heart of Showduino.

Responsibilities:

- Show runtime
- Timeline execution
- Scene state
- Trigger processing
- Safety state
- Audio/video orchestration
- Ethernet services
- USB host services
- Storage
- Device/plugin management
- Diagnostics and logging

The P4 must keep the show running even when the Director is offline.

## Ethernet

Ethernet is the primary fixed-installation network.

Planned uses:

- Art-Net
- sACN / E1.31
- OSC
- WebUI
- Show deployment
- Multi-Stage-Engine synchronization
- Logging
- Diagnostics
- Remote control from desktop tools
- Firmware and asset transfer

ESP-NOW handles portable operator control. Ethernet handles professional show infrastructure.

## USB Host

USB expands the Stage Engine beyond fixed GPIO.

Planned uses:

- USB flash drives
- Show import/export
- Backup and restore
- MIDI controllers
- USB audio interfaces
- Keyboards and mice
- Gamepads and operator panels
- USB serial devices
- Service tools

## Safety principles

- Stage Engine is authoritative.
- Emergency stop has highest priority.
- Active shows continue if the Director disconnects.
- Every important command is acknowledged.
- Wired control paths remain available for service and recovery.
- Hardware-specific code stays behind clean interfaces.

## Repository direction

```text
showduino/
├── director/
│   └── esp32s3-5inch/
├── stage-engine/
│   └── esp32-p4/
├── bridge/
│   └── esp32-c6/
├── protocol/
├── runtime/
├── plugins/
│   ├── dmx/
│   ├── artnet/
│   ├── sacn/
│   ├── audio/
│   ├── midi/
│   ├── gpio/
│   └── relay/
├── docs/
└── tests/
```

The existing `firmware/` folders remain valid during migration. New shared components should use the platform-oriented structure above.
