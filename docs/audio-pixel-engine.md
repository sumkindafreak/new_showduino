# Showduino Audio and Pixel Engine

The P4 onboard ES8311 speaker provides Showduino system and safety audio only. Attraction/programme audio is exclusively produced by specialist Audio Nodes.

## 1. Audio architecture

### A. Showduino system / safety audio — P4 onboard ES8311

The Waveshare ESP32-P4-Module-DEV-KIT includes:

```text
ES8311 codec @ I²C 0x18
NS4150B power amplifier (PA_Ctrl GPIO53, active HIGH)
MX1.25 8Ω / 2W speaker connector
onboard microphone (unused by system audio)
```

This path is **never** an attraction/programme player. There is no Audio Node → P4 speaker fallback.

```text
1. BOOT
2. EMERGENCY
3. BEEP
4. TONE
5. ERROR
6. ACCEPTED
7. COMPLETE
8. SHUTDOWN     RESERVED — not implemented
```

Canonical SD files (`/showduino/audio/system/`):

```text
boot.wav
emergency.wav
beep.wav
tone.wav
error.wav
accepted.wav
complete.wav
shutdown.wav     present-ok / unused
```

Known-good WAV: PCM, 16-bit, mono or stereo, 44.1 kHz or 48 kHz preferred, 32 kHz accepted for existing emergency assets. Headers are validated. Unsupported encodings are rejected. Playback is not claimed until codec + I2S + amplifier + a valid file + I2S bytes flowing are all true.

Verified I2S mapping (Waveshare wiki + ESP-IDF P4 examples):

```text
I2C SDA     GPIO7
I2C SCL     GPIO8
I2S DSDIN   GPIO9     P4 DOUT → ES8311 DAC
I2S LRCK    GPIO10    not a status LED
I2S ASDOUT  GPIO11    unused for playback
I2S SCLK    GPIO12
I2S MCLK    GPIO13
PA enable   GPIO53
```

The old external PCM5102A path (GPIO20/21/22) is retired and compiled out.

Priority (local P4 only):

```text
EMERGENCY
    ↓
ERROR
    ↓
other system notifications
```

Emergency always wins. Clearing emergency stops the WAV and returns IDLE (no resume). Emergency safety does not depend on `emergency.wav` playing.

### B. Show/programme audio — Audio Node (ESP32-A1S / ES8388)

Theatrical and production audio now belongs to the first specialist **Audio Node**:

```text
music
voice / dialogue
ambience
scare SFX
stingers
timeline audio (future cue dispatch)
```

The Node stores WAV files on its own microSD and reports a confirmed lifecycle. P4 decides; Comms transports; the Node plays.

The retired P4 PCM5102A path is not a substitute for the Audio Node.

See [`docs/audio-node.md`](audio-node.md).

## 2. I2S ownership

The ESP32-P4 has one I2S peripheral. It is owned by onboard ES8311 system audio.

Attraction audio does not share this peripheral. The Audio Node has its own codec and SD.

## 3. Audio storage

Recommended P4 SD layout:

```text
/showduino/audio/system/
  boot.wav
  emergency.wav
  beep.wav
  tone.wav
  error.wav
  accepted.wav
  complete.wav
  shutdown.wav     present-ok / unused

/showduino/audio/show_machine/
  same filenames — used when the system/ copy is missing or not a valid engine WAV
```

Valid engine WAV: PCM or WAVE_FORMAT_EXTENSIBLE PCM, 16-bit, mono or stereo, 32 / 44.1 / 48 kHz. Missing or invalid system sounds must never stop the Show Engine booting.

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

The P4 local pixel baseline is now:

```text
P4
├── Emergency NeoPixel line (GPIO24, existing)
└── General Show Pixel Line ×1 (planned GPIO23 — not implemented)
Future expansion
└── Pixel / LED Nodes
    ├── additional strips
    ├── segmented effects
    └── zone-specific pixels
```

One properly implemented P4 show-pixel line is preferable to several unfinished local lines. Do not add more P4 show-pixel outputs in this generation.

That single planned show line should later support:

- Sub-strip / segment effects
- Multiple simultaneous segments on one physical line
- Brightness and colour control
- Speed and direction
- Duration
- Layer/lane behaviour in Studio

Example desired use on the one local show line:

```text
Show Pixel Line:
  pixels 0-7   → LIGHTNING
  pixels 8-10  → SOLID BLUE
  pixels 11+   → WARM WHITE GLOW
```

`PIXEL:` commands currently reply `UNSUPPORTED:PIXEL`. HELLO still reports `PIXELS:PLANNED`.

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

1. Initialise the onboard ES8311. Play `boot.wav` when the Director touchscreen sends its first `HELLO` after being absent (screen power-on), not when the P4 itself boots — implemented; hardware confirmation required.
2. Retired: PCM5102A show path on GPIO20/21/22. Attraction audio is Audio-Node-only.
3. Local priority: EMERGENCY > ERROR > other system notifications.
4. Missing system-sound files cannot block startup.
5. Emergency latch stops Audio Node programme audio and loops P4 `emergency.wav` independently.
6. WebUI System page shows P4 SYSTEM AUDIO health. Outputs page keeps the Audio Node.

## 11. Hardware split decision

```text
P4 Show Engine
├── onboard ES8311 → Showduino system/safety sounds only
├── local emergency pixel line (GPIO24)
├── one planned local show-pixel line (not implemented)
└── authoritative routing/state for specialist nodes

Specialist Nodes
├── Audio Node (ESP32-A1S / ES8388) → all attraction / programme audio
└── Pixel Node → future distributed theatrical pixels
```

**Audio Node — IMPLEMENTED / HARDWARE TEST REQUIRED.** Relay / MOSFET / LED / Pixel Nodes remain future.
