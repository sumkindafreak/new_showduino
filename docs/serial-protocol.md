# Showduino v1 Serial Protocol

This document defines the first clean Showduino v1 command protocol.

The protocol should be easy to read in Serial Monitor and easy to parse on Arduino/ESP32.

## Transport

Initial transport:

```text
CYD Controller ESP32  <---- UART Serial ---->  Arduino Mega Executor
```

Future transports:

- USB serial
- WiFi HTTP bridge
- WebSocket
- ESP-NOW bridge
- MQTT-style bridge if needed later

## Serial Settings

Recommended default:

```text
Baud: 115200
Line ending: \n
Encoding: ASCII / UTF-8 safe text
```

Every command should end with a newline.

## Command Style

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
MP3:A:PLAY:001
PIXEL:1:EFFECT:FIRE
SHOW:PLAY
EMERGENCY:STOP
```

## Response Style

The executor should respond to important commands.

Examples:

```text
OK:RELAY:1:ON
OK:RELAY:1:PULSE:3000
ERR:RELAY:9:INVALID_RELAY
STATUS:READY
STATUS:EMERGENCY_ACTIVE
```

## Heartbeat

Controller sends:

```text
HEARTBEAT
```

Executor replies:

```text
STATUS:ALIVE
```

If heartbeat fails repeatedly, the controller should show a communication warning.

## Emergency Commands

Emergency stop must always be processed first.

```text
EMERGENCY:STOP
EMERGENCY:CLEAR
```

Expected executor behaviour for `EMERGENCY:STOP`:

- Stop show playback
- Turn off all relays
- Stop relay pulses
- Blackout DMX if configured
- Stop or pause audio if configured
- Stop dangerous pixel/effect outputs if configured
- Report emergency active

Expected response:

```text
STATUS:EMERGENCY_ACTIVE
```

Expected behaviour for `EMERGENCY:CLEAR`:

- Clear emergency state
- Keep outputs off until new command is received
- Report ready

Expected response:

```text
STATUS:READY
```

## Relay Commands

```text
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:3000
RELAY:ALL:OFF
```

Rules:

- Relay numbers are 1-based.
- Pulse value is milliseconds.
- Maximum pulse length should be limited in firmware.
- Relays must boot OFF.

Responses:

```text
OK:RELAY:1:ON
OK:RELAY:1:OFF
OK:RELAY:1:PULSE:3000
OK:RELAY:ALL:OFF
ERR:RELAY:9:INVALID_RELAY
```

## DMX Commands

Single channel:

```text
DMX:1:255
```

Range:

```text
DMX:1-10:0
```

Blackout:

```text
DMX:BLACKOUT
```

Responses:

```text
OK:DMX:1:255
OK:DMX:1-10:0
OK:DMX:BLACKOUT
ERR:DMX:600:INVALID_CHANNEL
ERR:DMX:1:300:INVALID_VALUE
```

Rules:

- DMX channels are 1-512.
- DMX values are 0-255.

## MP3 / Audio Commands

Two-player executor style:

```text
MP3:A:PLAY:001
MP3:A:STOP
MP3:A:PAUSE
MP3:A:VOLUME:20
MP3:B:PLAY:004
```

Single-player SUE / I2S style:

```text
AUDIO:PLAY:/audio/thunder.mp3
AUDIO:STOP
AUDIO:PAUSE
AUDIO:RESUME
AUDIO:VOLUME:80
AUDIO:STATUS
```

Responses:

```text
OK:MP3:A:PLAY:001
OK:MP3:A:STOP
OK:AUDIO:PLAY:/audio/thunder.mp3
ERR:MP3:C:INVALID_PLAYER
ERR:AUDIO:NO_SD
```

## Pixel Commands

Basic:

```text
PIXEL:1:ON
PIXEL:1:OFF
PIXEL:1:COLOR:255,0,0
PIXEL:1:BRIGHTNESS:128
```

Effect:

```text
PIXEL:1:EFFECT:FIRE
PIXEL:1:EFFECT:PULSE
PIXEL:1:EFFECT:STROBE
```

Sub-strip concept:

```text
PIXEL:1:EFFECT:FIRE:START:0:COUNT:30:BRIGHTNESS:180:SPEED:40
```

Responses:

```text
OK:PIXEL:1:EFFECT:FIRE
ERR:PIXEL:5:INVALID_LINE
```

## Show Commands

```text
SHOW:LOAD:chamber_demo.shdo
SHOW:PLAY
SHOW:PAUSE
SHOW:STOP
SHOW:STATUS
SHOW:LIST
```

Responses:

```text
OK:SHOW:LOAD:chamber_demo.shdo
OK:SHOW:PLAY
OK:SHOW:STOP
STATUS:SHOW:PLAYING:chamber_demo.shdo
ERR:SHOW:FILE_NOT_FOUND
```

## Input / Sensor Reports

Executor may report inputs back to controller:

```text
INPUT:1:HIGH
INPUT:1:LOW
ANALOG:1:512
TRIGGER:DOOR:OPEN
TRIGGER:PIR:ACTIVE
```

## Status Reports

Recommended periodic report:

```text
STATUS:READY
STATUS:BUSY
STATUS:EMERGENCY_ACTIVE
STATUS:SHOW_PLAYING
STATUS:SD_OK
STATUS:SD_FAIL
STATUS:DMX_OK
STATUS:MP3_A_OK
STATUS:MP3_B_OK
```

## Legacy Compatibility

Older repos used command styles such as:

```text
RELAY_1_ON
RELAY_1_OFF
RELAY_1_ON_TIMER=3000
MP3_1_PLAY_001
DMX_BLACKOUT
STATUS_REQUEST
```

Showduino v1 may support these through a compatibility parser, but new code should use the colon format.

## Parser Rules

- Trim whitespace.
- Ignore empty lines.
- Convert command category to uppercase.
- Validate all numbers.
- Never block while parsing.
- Emergency stop must be checked before long actions.
- Unknown commands return `ERR:UNKNOWN_COMMAND`.

## First Required Commands for v1 Demo

The first working demo only needs:

```text
HEARTBEAT
STATUS:ALIVE
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:1000
RELAY:ALL:OFF
EMERGENCY:STOP
EMERGENCY:CLEAR
STATUS:READY
```

Everything else comes after this baseline is reliable.
