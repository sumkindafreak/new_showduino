# Showduino v1 Scene Creator Design

The Scene Creator is the most important part of Showduino v1.

Relays, DMX, pixels, and audio are outputs. The Scene Creator is where a scare attraction designer actually builds the moment.

## Core Idea

A user should be able to build a scene visually, save it to SD card, and play it back reliably without needing to code.

A scene is a timeline made from cues.

Each cue can trigger:

- Pixel effects
- Local SD audio
- Relays
- DMX
- Delays
- Loops
- Sensor waits
- Add-on node commands

## Design Priority

Showduino v1 should prioritise:

1. Scene Creator
2. Pixel FX engine
3. Local SD audio playback
4. Timeline playback
5. Relays
6. DMX
7. Add-on nodes
8. Advanced web features

## User Workflow

The dream workflow:

1. Open GoreFX / Showduino dashboard.
2. Create a new scene.
3. Add an audio file from SD card.
4. Add pixel effects on a timeline.
5. Add relay hits, strobes, fog, props, or DMX cues.
6. Preview the scene.
7. Save it as a `.shdo` file.
8. Send it to the controller SD card.
9. Trigger it from touchscreen, web dashboard, sensor, or button.

## Scene Creator Views

### 1. Timeline View

A horizontal timeline showing time in seconds.

Tracks:

```text
Audio Track
Pixel Track 1
Pixel Track 2
Pixel Track 3
Pixel Track 4
Relay Track
DMX Track
Trigger Track
Notes Track
```

### 2. Cue Inspector

When a cue is selected, show editable fields:

- Cue name
- Start time
- Duration
- Target output
- Effect type
- Colour
- Brightness
- Speed
- Audio file
- Relay number
- DMX channel/value
- Fade in/out
- Loop mode

### 3. Pixel Preview

A simple visual strip preview showing what the selected pixel effect should roughly look like.

### 4. Audio Browser

List audio files from SD card:

```text
/audio/thunder.mp3
/audio/scream.wav
/audio/heartbeat.mp3
/audio/scene01_intro.mp3
```

### 5. Test Buttons

Every cue should have quick test buttons:

```text
TEST CUE
TEST FROM HERE
STOP
EMERGENCY STOP
```

## Scene File Format

Working extension:

```text
.shdo
```

Recommended format:

```json
{
  "format": "showduino-scene",
  "version": 1,
  "name": "Chamber Intro",
  "description": "Opening scene for The Chamber",
  "duration_ms": 30000,
  "audio": [
    {
      "id": "intro_audio",
      "time_ms": 0,
      "file": "/audio/chamber_intro.mp3",
      "volume": 85
    }
  ],
  "pixels": [
    {
      "id": "portal_glow",
      "time_ms": 0,
      "duration_ms": 10000,
      "line": 1,
      "effect": "PORTAL_GLOW",
      "start": 0,
      "count": 60,
      "color": [0, 180, 255],
      "brightness": 180,
      "speed": 40
    }
  ],
  "relays": [
    {
      "id": "fog_burst",
      "time_ms": 4500,
      "relay": 1,
      "action": "PULSE",
      "duration_ms": 1500
    }
  ],
  "dmx": [],
  "triggers": []
}
```

## Cue Types

### Audio Cue

```json
{
  "time_ms": 0,
  "type": "AUDIO",
  "file": "/audio/intro.mp3",
  "volume": 80
}
```

### Pixel Cue

```json
{
  "time_ms": 500,
  "type": "PIXEL",
  "line": 1,
  "effect": "FIRE",
  "duration_ms": 5000,
  "start": 0,
  "count": 60,
  "color": [255, 80, 0],
  "brightness": 200,
  "speed": 50
}
```

### Relay Cue

```json
{
  "time_ms": 2000,
  "type": "RELAY",
  "relay": 1,
  "action": "PULSE",
  "duration_ms": 1000
}
```

### DMX Cue

```json
{
  "time_ms": 2500,
  "type": "DMX",
  "channel": 1,
  "value": 255
}
```

## Pixel FX Requirements

Each pixel effect must support:

```text
line
start
count
color
speed
brightness
duration_ms
reverse
```

This allows one LED strip to be split into different areas.

Example:

```text
PIXEL:1:EFFECT:FIRE:START:0:COUNT:30:BRIGHTNESS:180:SPEED:40
```

## Local Audio Requirements

The long-term audio direction is local SD playback using an ESP32 with PCM5102A I2S audio.

Audio files should live on SD card:

```text
/audio/
/scenes/
/shows/
```

Supported target formats:

- MP3
- WAV

Preferred for reliable effects:

- Short MP3 for ambience/speech/music
- WAV for instant scare hits if latency becomes an issue

## Hardware Direction for Audio

Preferred audio node:

```text
ESP32-S3 + SD card + PCM5102A I2S DAC
```

This avoids depending on small serial MP3 boards for the final product.

## Dashboard Scene Creator Features

Minimum v1 Scene Creator:

- Create scene
- Add audio cue
- Add pixel cue
- Add relay cue
- Edit cue timing
- Save `.shdo`
- Export `.shdo`
- Import `.shdo`
- Send command preview

Later features:

- Drag-and-drop cues
- Waveform display
- Pixel simulation
- Multi-scene show builder
- Sensor-triggered branches
- Effect marketplace/community library

## Touchscreen Scene Control

The CYD touchscreen does not need to be a full editor at first.

Initial CYD role:

- List scenes from SD
- Load scene
- Play scene
- Stop scene
- Emergency stop
- Basic manual test controls

Full scene creation should begin in the web dashboard because the screen is larger.

## First Scene Creator Milestone

The first real Showduino v1 creative milestone should be:

```text
Create a 10-second scene that plays local audio and runs a pixel effect in sync.
```

Example:

```text
0ms     AUDIO:PLAY:/audio/heartbeat.mp3
0ms     PIXEL:1:EFFECT:PULSE red
3000ms  PIXEL:1:EFFECT:BUILD blue
7000ms  RELAY:1:PULSE:1000
9000ms  PIXEL:1:EFFECT:BLACKOUT
10000ms SCENE:END
```

This proves the core product.

## Final Design Statement

Showduino is not just a relay controller.

Showduino v1 is a scene creation system for scare attractions where audio, pixels, timing, and physical effects can be designed together.
