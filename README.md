# Showduino v1

**Showduino v1** is a modular show-control platform for scare attractions, escape rooms, immersive experiences, and interactive props.

## Current Architecture

```text
Phone / Tablet / Laptop
        │
     Browser
        │
ESP32-S3 Director (5" Touchscreen + WebUI)
        │ UART
ESP32-P4 Stage Engine
        │ UART
ESP32-C6/C3 ESP-NOW Bridge
        │ ESP-NOW
ESP32 Relay Nodes / Future Nodes
```

## Core Components

### Director (ESP32-S3)
- 5" touchscreen interface
- Hosts the GoreFX Studio WebUI
- Project management
- Timeline and Flow editors
- Show library
- Diagnostics
- OTA
- Sends commands to the Stage Engine

### Stage Engine (ESP32-P4)
- Deterministic show execution
- Timeline engine
- DMX
- NeoPixels
- Audio
- Sensors
- Safety system
- Emergency stop
- Routes relay commands to wireless nodes

### ESP-NOW Bridge
- UART connection to the Stage Engine
- ESP-NOW gateway
- Routes commands to wireless nodes
- Returns acknowledgements and status

### Relay Nodes
- ESP32 relay modules
- Wireless over ESP-NOW
- Local emergency handling
- Expandable to multiple nodes

## Repository Layout

```text
firmware/
 ├── director-s3/
 ├── stage-engine-p4/
 ├── espnow-bridge/
 ├── relay-node-esp32/
 └── future-nodes/

web/
 └── gorefx-dashboard/

docs/
 ├── architecture.md
 ├── command-protocol.md
 ├── show-file-format.md
```

## Current Progress

✅ React + TypeScript GoreFX dashboard foundation

✅ Shared Showduino command model

✅ Shared show file model

✅ ESP32-S3 Director UART scaffold

✅ ESP32-P4 Stage Engine scaffold

✅ ESP-NOW Bridge scaffold

✅ ESP32 Relay Node scaffold

✅ Locked modular architecture

## Long-Term Vision

Showduino is designed as a hardware-independent platform.

The WebUI, command protocol and show files remain stable while hardware evolves.

Current hardware targets:
- ESP32-S3 Director
- ESP32-P4 Stage Engine
- ESP32-C6/C3 Bridge
- ESP32 Relay Nodes

Future nodes can include lighting, audio, sensors, puzzles, animatronics and custom props using the same command language.

The goal is a scalable ecosystem where attractions can grow from a single controller to distributed wireless systems without changing how shows are created.
