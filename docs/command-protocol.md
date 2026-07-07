# Showduino Command Protocol

This is the first shared command protocol for Showduino v1.

It is designed to work over:

- USB serial
- UART between CYD/controller and executor
- WiFi HTTP/WebSocket bridge
- Mock mode inside the WebUI
- Future ESP-NOW bridge nodes

## Principles

1. Commands should be human-readable.
2. Commands should be short enough for serial links.
3. Commands should map cleanly to WebUI buttons and timeline cues.
4. Emergency commands must always be available.
5. Unknown commands should be rejected safely and logged.

## Core commands

### Show control

```text
SHOW:LOAD:<ShowName>
SHOW:START
SHOW:STOP
SHOW:PAUSE
SHOW:RESUME
```

### Relay control

```text
RELAY:<Channel>:ON
RELAY:<Channel>:OFF
RELAY:<Channel>:PULSE:<Milliseconds>
RELAY:ALL:OFF
```

Examples:

```text
RELAY:1:ON
RELAY:1:OFF
RELAY:4:PULSE:2500
RELAY:ALL:OFF
```

### Audio control

```text
AUDIO:<Player>:PLAY:<TrackNumber>
AUDIO:<Player>:STOP
AUDIO:<Player>:PAUSE
AUDIO:<Player>:VOLUME:<0-30>
```

Examples:

```text
AUDIO:1:PLAY:014
AUDIO:2:VOLUME:25
```

### DMX control

```text
DMX:<Channel>:<Value>
DMX:RANGE:<StartChannel>:<EndChannel>:<Value>
DMX:BLACKOUT
```

Examples:

```text
DMX:10:255
DMX:RANGE:1:12:0
DMX:BLACKOUT
```

### NeoPixel / pixel effects

```text
PIXEL:<EffectName>
PIXEL:<Line>:<EffectName>
PIXEL:<Line>:BRIGHTNESS:<0-255>
PIXEL:ALL:OFF
```

Examples:

```text
PIXEL:HELLFIRE
PIXEL:1:RAINBOW
PIXEL:2:BRIGHTNESS:120
PIXEL:ALL:OFF
```

### Status and diagnostics

```text
STATUS:REQUEST
DIAG:SELF_TEST
DIAG:RELAY_TEST:<Channel>
DIAG:DMX_TEST
```

### Emergency

```text
EMERGENCY:STOP
EMERGENCY:CLEAR
```

Emergency stop should immediately:

- Stop all running timelines
- Turn off relays where safe
- Stop dangerous outputs
- Blackout DMX where configured
- Stop or lower audio if required
- Tell all connected controllers/nodes the system is locked

## Response examples

```text
OK:RELAY:1:ON
OK:AUDIO:1:PLAY:014
ERR:UNKNOWN_COMMAND
ERR:INVALID_CHANNEL
STATUS:READY
STATUS:EMERGENCY_LOCKED
```

## Future protocol layers

Later versions may add:

- Checksums for noisy serial links
- JSON command envelopes for WebSocket use
- Message IDs for ACK/retry
- Node IDs for distributed ESP-NOW prop nodes
- Capability discovery from connected modules
