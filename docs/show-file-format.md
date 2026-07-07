# Showduino Show File Format

This is the first shared show/project format for Showduino v1.

The goal is to let the WebUI, CYD touchscreen, executor firmware, SD card tools and future add-on nodes understand the same show structure.

## Design rules

1. The WebUI is the main editing environment.
2. The CYD is a field controller and show launcher.
3. The executor receives simple commands.
4. The same project can become a `.shdo` file later.
5. Emergency stop must override all timelines, flow blocks and manual commands.

## Starter JSON structure

```json
{
  "id": "demo-zombie-burst",
  "showName": "Zombie Burst",
  "version": "0.1.0",
  "description": "Starter timed scare sequence for testing relays, audio, pixels and fog.",
  "createdAt": "2026-07-07T00:00:00.000Z",
  "updatedAt": "2026-07-07T00:00:00.000Z",
  "steps": [
    {
      "id": "step-audio-thunder",
      "type": "audio",
      "name": "Play thunder",
      "timeMs": 0,
      "durationMs": 4000,
      "command": "AUDIO:1:PLAY:014",
      "notes": "Player 1 starts the scare bed."
    }
  ]
}
```

## Step fields

| Field | Type | Purpose |
| --- | --- | --- |
| `id` | string | Stable unique ID for the editor and logs |
| `type` | string | `relay`, `audio`, `dmx`, `pixel`, `delay`, `sensor`, `command` |
| `name` | string | Human-readable cue name |
| `timeMs` | number | Timeline start time in milliseconds |
| `durationMs` | number | Optional cue length |
| `command` | string | Command sent to executor/controller |
| `notes` | string | Optional notes for builders/operators |

## Early command examples

```text
SHOW:LOAD:ZombieBurst
SHOW:START
SHOW:STOP
RELAY:1:ON
RELAY:1:OFF
RELAY:1:PULSE:500
AUDIO:1:PLAY:014
AUDIO:1:STOP
DMX:10:255
PIXEL:HELLFIRE
STATUS:REQUEST
EMERGENCY:STOP
EMERGENCY:CLEAR
```

## Future `.shdo` direction

The `.showduino.json` editor file can later export to `.shdo` for SD card loading.

Possible future options:

- Plain JSON `.shdo`
- Minified JSON `.shdo`
- Line-based command `.shdo`
- Binary packed format for low-memory boards

For now, keep it readable JSON so debugging is easy.
