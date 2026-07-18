# Showduino v1 Mega Executor Firmware

```text
Status: ARCHIVE CANDIDATE
Not part of the canonical runtime.
```

See [`docs/repository-status.md`](../../docs/repository-status.md).

This folder contains Arduino Mega 2560 executor firmware from the pre–Show Engine stack.

## Purpose

The Mega executor is the first Showduino v1 hardware output brain.

It should control:

- 8 relays
- DMX output
- MP3/audio modules
- NeoPixel lines
- Digital inputs
- Analog inputs
- Emergency stop behaviour
- Status reports to the controller

## Source References

Primary source repos:

```text
sumkindafreak/arduino_show_controller
sumkindafreak/showduino-complete-code
sumkindafreak/showduino-scare-control-system
```

## Initial v1 Scope

The first executor build should only do this:

1. Boot with all relays OFF
2. Listen on Serial1 or chosen controller UART
3. Parse newline commands
4. Respond to `HEARTBEAT` with `STATUS:ALIVE`
5. Run `RELAY:1:ON`
6. Run `RELAY:1:OFF`
7. Run `RELAY:1:PULSE:1000`
8. Run `RELAY:ALL:OFF`
9. Run `EMERGENCY:STOP`
10. Run `EMERGENCY:CLEAR`
11. Print debug messages over USB Serial

Only after this works should we add DMX, MP3, NeoPixels, inputs, and show files.

## Planned Modules

```text
executor-mega.ino
config.h
command_parser.h / command_parser.cpp
relay_control.h / relay_control.cpp
emergency_control.h / emergency_control.cpp
status_report.h / status_report.cpp
dmx_control.h / dmx_control.cpp
audio_control.h / audio_control.cpp
pixel_control.h / pixel_control.cpp
input_control.h / input_control.cpp
```

## Pin Draft

See:

```text
docs/hardware-pinout.md
```

## Safety Rules

- Relays must boot OFF.
- Emergency stop must override every other command.
- Relay pulse timers must be non-blocking.
- Serial command parsing must be non-blocking.
- Unknown commands must return an error.
- Debug output must explain what the executor is doing.
