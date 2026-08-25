# Showduino Audio and Pixel Engine

Audio and pixels are core Showduino outputs. The Stage Controller now separates **system audio** from **show audio** by purpose while keeping the emergency pixel path local and deterministic.

## 1. Audio architecture

### A. Showduino/system sounds — onboard ES8311

The Waveshare ESP32-P4-Module-DEV-KIT already includes:

```text
ES8311 codec
NS4150B power amplifier
onboard microphone
8Ω / 2W speaker header
```

Showduino reserves this path for short local sounds that belong to the controller itself:

```text
boot
ready
production loaded
show armed
link warning
error
emergency acknowledgement
restart / shutdown
```

Official board mapping:

```text
I2C SDA     GPIO7
I2C SCL     GPIO8
I2S DSDIN  GPIO9
I2S LRCK   GPIO10
I2S ASDOUT GPIO11
I2S SCLK   GPIO12
I2S MCLK   GPIO13
PA enable  GPIO53
```

System sounds should be small, fast to load and available without relying on a separate audio node.

### B. Show/programme audio — external PCM5102A

The external PCM5102A remains part of the Stage Controller because show audio is a separate responsibility.

Use it for:

```text
music
voice / dialogue
ambience
scare SFX
timeline audio
show emergency track where required
```

Current wiring:

```text
WS/LRCK  GPIO20
BCLK     GPIO21
DOUT     GPIO22
```

The PCM path is the primary line-level show-audio output.

## 2. I2S arbitration rule

The ESP32-P4 has one I2S peripheral.

Therefore the two audio paths are **logically separate roles**, not a promise of two fully independent simultaneous hardware streams.

V1 policy:

- Show audio has priority while a timeline audio stream is active.
- A routine system sound must not interrupt or corrupt active show audio.
- Boot/ready/loaded sounds normally occur while show audio is idle.
- Emergency policy may deliberately stop show audio before playing an emergency/system acknowledgement where required.
- Any future shared-bus or rapid-switching implementation must be tested before simultaneous behaviour is claimed.

## 3. Audio storage

Recommended P4 SD layout:

```text
/showduino/audio/system/
  boot.wav
  ready.wav
  loaded.wav
  armed.wav
  warning.wav
  error.wav

/showduino/audio/show/
  ambience/
  music/
  dialogue/
  sfx/

/showduino/shows/
  packages/
```

System sound names should be configurable; missing system sounds must never stop the Show Engine booting.

## 4. Audio command model

Show-level commands should remain explicit about the output role.

Examples:

```text
AUDIO:SHOW:PLAY:/showduino/audio/show/sfx/thunder.wav
AUDIO:SHOW:STOP
AUDIO:SHOW:VOLUME:80

AUDIO:SYSTEM:PLAY:ready
AUDIO:SYSTEM:PLAY:error
```

The exact wire protocol may evolve, but the distinction between `SHOW` and `SYSTEM` should remain so the scheduler can enforce I2S ownership safely.

## 5. Timeline audio

Show audio is timeline-first.

Example cue shape:

```json
{
  "time_ms": 0,
  "type": "AUDIO",
  "target": "show",
  "file": "/showduino/audio/show/ambience/chamber.wav",
  "volume": 85
}
```

Required behaviour:

- Start on cue.
- Allow pixel/output cues to overlap.
- Avoid blocking delays.
- Keep show timing independent of file-decoder blocking.
- Report playback faults to the Show Engine.

Perfect sample-accurate distributed sync is not required for the first release.

## 6. Pixel engine direction

Pixels are part of the show language, not decoration.

The pixel engine should support:

- Multiple named pixel lines
- Sub-strip / segment effects
- Multiple simultaneous segments on one physical line
- Brightness and colour control
- Speed and direction
- Duration
- Layer/lane behaviour in Studio

Example desired use:

```text
Pixel line 1:
  pixels 0-7   → LIGHTNING
  pixels 8-10  → SOLID BLUE
  pixels 11+   → WARM WHITE GLOW
```

## 7. Emergency pixel line

The Stage Controller's local emergency strip remains independent from normal show pixel assignments.

```text
DATA GPIO24
```

Normal Showduino Studio timelines must not treat the emergency line as an ordinary editable show-output lane.

## 8. Pixel effect vocabulary

Initial effects:

```text
OFF
SOLID
FADE_IN
FADE_OUT
PULSE
FLICKER
FIRE
STROBE
LIGHTNING
CHASE
BUILD
PORTAL_GLOW
WARNING_RED
BLACKOUT
```

Preferred effect parameters:

```text
line
start
count
color
speed
brightness
reverse
duration_ms
```

## 9. Example audio + pixel scene

```json
{
  "name": "Pixel Audio Test",
  "duration_ms": 10000,
  "cues": [
    {
      "time_ms": 0,
      "type": "AUDIO",
      "target": "show",
      "file": "/showduino/audio/show/ambience/heartbeat.wav",
      "volume": 80
    },
    {
      "time_ms": 0,
      "type": "PIXEL",
      "line": 1,
      "start": 0,
      "count": 8,
      "effect": "PULSE",
      "color": [255, 0, 0],
      "brightness": 150,
      "speed": 40,
      "duration_ms": 4000
    },
    {
      "time_ms": 4000,
      "type": "PIXEL",
      "line": 1,
      "start": 8,
      "count": 3,
      "effect": "SOLID",
      "color": [0, 120, 255],
      "brightness": 200,
      "duration_ms": 3000
    }
  ]
}
```

## 10. First audio milestone under the new baseline

1. Initialise the onboard ES8311 and play a short boot/ready WAV through the board speaker.
2. Verify the PCM5102A show path on GPIO20/21/22.
3. Add an audio-resource owner/state machine so only the permitted path owns I2S at a time.
4. Confirm a missing system-sound file cannot block startup.
5. Confirm emergency handling can stop show audio deterministically.
6. Add Studio/runtime status showing `SYSTEM`, `SHOW`, `IDLE`, or `FAULT` audio ownership.

## 11. Hardware split decision

```text
P4 Show Engine
├── onboard ES8311 → Showduino/system sounds
├── external PCM5102A → show/programme audio
├── local emergency pixel line
└── dispatch to specialist remote nodes as required
```

Audio nodes remain useful for remote zones and multi-room systems, but they are no longer required just to give the Stage Controller basic system audio.
