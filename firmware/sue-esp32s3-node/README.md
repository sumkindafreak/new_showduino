# Showduino v1 SUE ESP32-S3 Node Firmware

```text
Status: INCOMPLETE
Placeholder / stub; not operational production firmware.
```

See [`docs/repository-status.md`](../../docs/repository-status.md).

This folder is reserved for SUE / ESP32-S3 add-on node firmware.

## Purpose

SUE is a powerful Showduino add-on node.

It can become:

- Wireless prop node
- Wireless relay node
- Wireless audio node
- Wireless pixel node
- RTC/scheduled node
- Standalone mini controller

## Source Reference

Primary source repo:

```text
sumkindafreak/SHOWDUINO---SUE
```

Useful known features:

- Modular structure
- `config.h` pin mapping
- FastLED
- RTC DS3231/DS1307
- SD card
- Relay/PWM outputs
- PCM5102A I2S audio
- ESP-NOW communication

## Initial v1 Scope

SUE should not be the first v1 hardware baseline.

The first baseline is:

```text
CYD Controller → Mega Executor → Relay test
```

After that, SUE should be migrated as an add-on node.

## Planned Modules

```text
sue-esp32s3-node.ino
config.h
led_control.h / led_control.cpp
rtc_control.h / rtc_control.cpp
sd_card.h / sd_card.cpp
relay_control.h / relay_control.cpp
audio_control.h / audio_control.cpp
espnow_comms.h / espnow_comms.cpp
showduino_bridge.h / showduino_bridge.cpp
```

## v1 Design Decision

SUE should communicate using Showduino v1 command language where possible.

Example:

```text
AUDIO:PLAY:/audio/thunder.mp3
RELAY:1:ON
PIXEL:1:EFFECT:FIRE
STATUS:ALIVE
```

ESP-NOW packets can carry these same command strings or a compact structured version later.

## Safety Rules

- Outputs boot OFF.
- Node must fail safe if communication is lost.
- Audio must not block command handling.
- Relay pulse timers must be non-blocking.
- ESP-NOW receive callbacks must stay lightweight.
