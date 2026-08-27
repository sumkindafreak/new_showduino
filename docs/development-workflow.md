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

When the Director code is ready, it will be reviewed and integrated without rewriting the finished interface.

### Stage Engine workstream — Arduino P4 (current product path)

The supported Stage Engine for this hardware generation is:

```text
firmware/stage-engine-p4/
```

It talks to the dedicated ESP32-S3 Comms Controller over **UART**, not SDIO. Native ESP-IDF work under `stage-engine/esp32-p4/` remains a parallel backend experiment.

## Confirmed Stage Engine hardware

Target board:

**Waveshare ESP32-P4-Module-DEV-KIT**

Confirmed capabilities (board):

- ESP32-P4 with onboard PSRAM
- integrated ESP32-C6 (**UNUSED BY SHOWDUINO / RESERVED HARDWARE**)
- factory P4↔C6 SDIO (unused by Showduino commands)
- Wi-Fi 6 and Bluetooth 5 through the C6 **if** ESP-Hosted or similar is installed later
- Gigabit / onboard Ethernet (board feature; not required for the Director path)
- USB 2.0 OTG
- SDIO 3.0 microSD slot (Showduino SD card uses P4 SDMMC Slot 0, GPIO39–45)
- 40-pin GPIO expansion header

See [`docs/final-hardware-architecture.md`](final-hardware-architecture.md) for the locked pin map.

## Platform roles

### Director

The Director is an operator console. It sends intent, shows status, and can be disconnected without stopping an active show.

### ESP32-S3 Comms Controller (current)

The dedicated ESP32-S3 Dev Module runs `firmware/s3-comms-controller/` and owns:

- ESP-NOW with the Director
- forwarding newline-framed commands to the P4 over UART
- returning P4 UART lines to the Director over ESP-NOW

USB CDC on the S3 is for programming/debug. The P4 UART uses UART1 GPIO17/18, not UART0.

The Waveshare onboard C6 is unused reserved hardware. Factory SDIO and ESP-Hosted are **not** the current Director transport. See [`docs/future-p4-c6-sdio-transport.md`](future-p4-c6-sdio-transport.md).

**FUTURE / RESERVED / NOT IMPLEMENTED** on this S3: BLE, Wi-Fi SoftAP/STA, WebUI proxy, OTA.

### ESP32-P4 Stage Engine

The P4 owns:

- show runtime
- timeline execution
- authoritative show state
- local emergency handling (GPIO25)
- SD storage (GPIO39–45)
- show and emergency audio
- emergency NeoPixels
- WebUI origin/files
- runtime status

The P4 must never block indefinitely waiting for the communications controller.

## Immediate product path (locked)

```text
Touchscreen button
    ↓
Showduino command
    ↓
ESP-NOW from Director
    ↓
Dedicated ESP32-S3 Comms Controller
    ↓
UART 115200 8N1
    ↓
ESP32-P4 Stage Engine
```

Do not treat “implement SDIO transport” as the current integration milestone.

## Rules

1. The Stage Engine is authoritative.
2. An active show must continue if the Director disconnects.
3. The Director UI remains transport-independent.
4. ESP-NOW is the portable Director link into the dedicated S3 Comms Controller.
5. UART is the current P4↔S3 application transport.
6. Emergency commands receive the highest priority and remain local to the P4.
7. Board-specific code must be isolated from the shared Showduino protocol and runtime.
8. No unrelated robotics features belong in this project.
