# Showduino S3 Lamp Node

```text
Status: ACTIVE firmware / physical GPIOs confirmed
Role: Interactive carbide-lamp practical
Hardware: ESP32-S3 development board (same family as the Comms Controller)
Firmware: 0.3.4
```

This is the **production Lamp Node**.

It is **not** a Pixel Node, **not** an Audio Node, and **not** the historical C3 Super Mini OLED lamp (`firmware/c3-lamp-node/`, retained as reference).

```text
Director ESP32-S3
        │ ESP-NOW
        ▼
ESP32-S3 Communications Engine
        │ UART
        ▼
ESP32-P4 Show Engine
        │ ROUTE:LAMP:
        ▼
this S3 Lamp Node  (ESP-NOW)
```

The same firmware is a standalone interactive carbide lamp and a managed Showduino node. The P4 is authoritative only after GRANT. SoftAP `Showduino-Lamp-<id>` (typically `192.168.5.1`) hosts the local WebUI with no internet required. The node runs a local carbide state machine, jewel flame renderer, blow-to-extinguish detector, and optional Fermion DFPlayer Pro effect audio (`flick.mp3`, `fire_ignite.mp3`, `flameloop.mp3`, `emergency.mp3`).

## Sketch

```text
firmware/s3-lamp-node/ShowduinoS3LampNode/
```

This physical Lamp Node uses a CH343 USB-UART on UART0 (COM9 in the first bench). Application Serial must stay on UART0, not native USB CDC:

```text
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,FlashSize=8M,PSRAM=disabled,PartitionScheme=default_8MB" firmware/s3-lamp-node/ShowduinoS3LampNode
```

Production pins are in `BoardConfig.h` (`SHOWDUINO_LAMP_PINS_CONFIRMED = 1`).

## Hardware (physical audit)

Confirmed present:

- ESP32-S3
- 7-pixel NeoPixel Jewel (the lamp flame)
- analog microphone / sound sensor (blow to extinguish)
- light sensor (telemetry)
- voltage sensor (health)
- physical ignition / striker button
- planned DFRobot Fermion DFPlayer Pro DFR0768 (UART local FX)

Removed:

- PCM / I2S audio board
- separate amplifier

GPIOs: Jewel GPIO8, button GPIO7 (to GND), mic GPIO4, light GPIO5, voltage GPIO6, Fermion UART GPIO17/18. See [`docs/s3-lamp-node.md`](../../docs/s3-lamp-node.md).

## Protocol

`protocol/showduino_lamp_node.h` + `protocol/showduino_carbide_lamp.h`.

High-level examples:

```text
LAMP:NODE:LAMP-01:IGNITE
LAMP:NODE:LAMP-01:EXTINGUISH
LAMP:NODE:LAMP-01:FX:LOW_FLAME
LAMP:NODE:LAMP-01:FX:UNSTABLE
LAMP:NODE:LAMP-01:FX:FLARE
```

Existing `LAMP:FX:<token>` cues remain valid. SHDO package version stays **v2**.
