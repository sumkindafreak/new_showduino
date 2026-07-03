# Showduino v1 Audio and Pixel Engine

Audio and pixels are core to Showduino v1.

The system should be designed around local audio playback and rich LED effects, not just relay triggering.

## Audio Direction

Preferred final audio architecture:

```text
ESP32-S3 + SD Card + PCM5102A I2S DAC
```

Why:

- Audio can be stored locally on SD card.
- No need to rely on external serial MP3 modules as the main design.
- ESP32 can handle file browsing, volume, play/stop, and future sync logic.
- PCM5102A gives proper stereo line-level audio output.
- Audio nodes can become modular Showduino add-ons.

## Audio Board Role

The PCM audio board should become the standard Showduino audio output module.

Possible setups:

### Option A — Audio inside SUE node

```text
SUE ESP32-S3
├── SD card
├── PCM5102A I2S DAC
├── Pixel output
├── Relay/PWM output
└── ESP-NOW / WiFi command input
```

Best for:

- Local scene props
- Wireless rooms
- Self-contained effects
- Future product module

### Option B — Dedicated audio node

```text
ESP32-S3 Audio Node
├── SD card
├── PCM5102A I2S DAC
└── WiFi / ESP-NOW command receiver
```

Best for:

- Multi-room audio
- Ambience zones
- Independent speaker outputs

### Option C — Controller audio preview only

The CYD controller may preview short sounds later, but should not be the main show audio output.

## Audio File Layout

Recommended SD card layout:

```text
/audio/
  thunder.mp3
  scream.wav
  heartbeat.mp3
  ambience_loop.mp3

/scenes/
  chamber_intro.shdo
  whitechapel_scare.shdo

/shows/
  chamber_full_show.shdo
```

## Audio Commands

Local audio command format:

```text
AUDIO:PLAY:/audio/thunder.mp3
AUDIO:STOP
AUDIO:PAUSE
AUDIO:RESUME
AUDIO:VOLUME:80
AUDIO:STATUS
AUDIO:LIST
```

Scene cue example:

```json
{
  "time_ms": 0,
  "type": "AUDIO",
  "target": "local",
  "file": "/audio/thunder.mp3",
  "volume": 85
}
```

## Audio Sync Requirements

For v1, perfect sample-accurate sync is not required.

Required:

- Start audio on cue.
- Start pixel effect on cue.
- Keep scene timing based on `millis()`.
- Allow audio and pixel cues to overlap.
- Avoid blocking delay-based playback logic.

Later:

- Waveform preview
- Audio markers
- Beat sync
- SMPTE/MIDI-style timing
- Multi-node sync

## Pixel Engine Direction

Pixels are not decoration. They are part of the scare design language.

The pixel engine should support:

- Multiple lines
- Sub-strip effects
- Brightness control
- Colour control
- Speed control
- Direction/reverse
- Duration
- Layer-like behaviour later

## Pixel Command Format

Simple commands:

```text
PIXEL:1:OFF
PIXEL:1:COLOR:255,0,0
PIXEL:1:BRIGHTNESS:180
PIXEL:1:EFFECT:FIRE
```

Advanced effect command:

```text
PIXEL:1:EFFECT:FIRE:START:0:COUNT:60:COLOR:255,80,0:BRIGHTNESS:200:SPEED:50:DURATION:5000
```

## Pixel Effect Function Shape

Each effect should eventually be written in a consistent shape:

```cpp
void fxFire(uint8_t line, uint16_t start, uint16_t count, uint32_t color, uint8_t speed, uint8_t brightness, bool reverse);
```

Preferred parameter set:

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

## Essential v1 Pixel Effects

The first pixel effects should be practical for scare scenes:

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

## Scene Timeline Priority

Audio and pixels should be timeline-first.

Example:

```json
{
  "name": "Pixel Audio Test",
  "duration_ms": 10000,
  "cues": [
    {
      "time_ms": 0,
      "type": "AUDIO",
      "file": "/audio/heartbeat.mp3",
      "volume": 80
    },
    {
      "time_ms": 0,
      "type": "PIXEL",
      "line": 1,
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
      "effect": "BUILD",
      "color": [0, 120, 255],
      "brightness": 200,
      "speed": 25,
      "duration_ms": 3000
    },
    {
      "time_ms": 7000,
      "type": "PIXEL",
      "line": 1,
      "effect": "STROBE",
      "color": [255, 255, 255],
      "brightness": 255,
      "speed": 80,
      "duration_ms": 1000
    },
    {
      "time_ms": 8000,
      "type": "PIXEL",
      "line": 1,
      "effect": "BLACKOUT",
      "duration_ms": 500
    }
  ]
}
```

## Hardware Split Decision

For the first v1 hardware proof:

```text
Mega = safe physical outputs such as relays
ESP32-S3/SUE = audio + pixels
Dashboard = scene creator
CYD = scene trigger/control panel
```

This prevents the Mega from being overloaded with things it is not best at.

## First Audio + Pixel Milestone

Build a standalone ESP32-S3 sketch that:

1. Mounts SD card.
2. Plays `/audio/test.mp3` through PCM5102A.
3. Runs a NeoPixel pulse effect at the same time.
4. Accepts serial command `SCENE:TEST`.
5. Uses no blocking delays during playback/effects.

This becomes the first real creative Showduino proof.
