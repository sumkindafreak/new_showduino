# Showduino Scene Creator / Studio Design

The Scene Creator is where a scare-attraction designer turns Showduino hardware into an actual performance.

The current direction is **Studio-first authoring** with the static frontend hosted by the dedicated S3 Communications Engine and authoritative production/runtime state owned by the P4 Show Engine.

## Core idea

A user should be able to build a show visually, store it on the P4 SD card, and run it without writing firmware.

A show is a timeline of cues targeted at logical outputs/resources.

For pixels, the primary authoring object is a **named segment**, not an individual LED range repeated in every cue.

## Current architecture for authoring

```text
Browser / laptop
    → S3 Communications Engine SoftAP
    → Showduino Studio frontend from S3 PROGMEM
    → P4 Web API
    → P4 production store + timeline + output engines
```

The browser is a client. The S3 is a transport/static-host layer. The P4 remains authoritative.

## Current creative priority

Current development order around the first real show is:

1. commission the P4 GPIO23 segmented Show Pixel Engine;
2. commission the Audio Node;
3. extend production/timeline cues so AUDIO and PIXEL can be driven by stored shows;
4. build the segment-first Studio authoring workflow around those proven engines;
5. next specialist Node: C3 Lamp;
6. then C3 Pixel;
7. then MOSFET Node.

The old Relay Node direction is superseded. DMX is parked/out of scope until explicitly revisited.

## Segment-first pixel workflow

A physical pixel line is divided into named areas once:

```text
P4 MAIN PIXELS — GPIO23 — example 80 pixels

0-7    lightning-zone
8-15   blue-alcove
16-35  fire-bed
36-55  warning-strip
56-79  candle-wall
```

Then a show timeline can say:

```text
00:02.000  lightning-zone → LIGHTNING
00:02.000  blue-alcove    → SOLID BLUE
00:04.500  fire-bed       → FIRE
00:07.000  candle-wall    → FADE_OUT
```

The show designer should not need to remember `start=16,count=20` every time they use `fire-bed`.

See `docs/studio-pixel-authoring.md`.

## Current Studio Outputs-page commissioning surface

The Studio source already includes a GPIO23 segment commissioning/editor surface with:

- segment slots;
- start/count;
- FX picker;
- primary/secondary colours;
- brightness;
- speed;
- intensity;
- randomness;
- reverse;
- duration;
- apply/start/stop/status;
- global brightness, blackout and test commands.

This is intentionally a **commissioning/runtime surface first**. Persistent named segment definitions and production `PIXEL` cues remain the next authoring/schema step.

## Shared pixel FX library

P4 and the future C3 Pixel Node share the vocabulary in `protocol/showduino_pixel_fx.h`.

Current 25 entries/aliases:

```text
OFF / BLACKOUT
SOLID
FADE_IN
FADE_OUT
PULSE
BREATHE
FLICKER
CANDLE
FIRE
LIGHTNING
STROBE
RANDOM_STROBE
CHASE
BOUNCE
COMET
WIPE
REVERSE_WIPE
BUILD
SPARKLE
TWINKLE
GLITCH
WARNING / WARNING_RED
PORTAL / PORTAL_GLOW
RAINBOW
CUSTOM_SEQUENCE
```

An FX is parameterised rather than duplicated into many colour/speed variants.

Typical parameters:

```text
primary colour
secondary colour
brightness
speed
intensity
randomness
reverse/direction
duration
```

## Studio views

### 1. Timeline

A horizontal time-based view. Tracks/lanes should represent logical resources rather than transport details.

Initial useful track families:

```text
Audio Node
P4 Pixel Segments
C3 Lamp                     when implemented
C3 Pixel Segments          when implemented
MOSFET Outputs             when implemented
Triggers / Inputs          as runtime support matures
Notes / Markers
```

Do not add DMX authoring while DMX remains parked.

### 2. Segment/layout editor

A pixel line should be shown as a strip divided into named segment blocks:

```text
MAIN SHOW PIXELS — 0..79

┌──────────┬──────────┬──────────────────┬──────────────────┬──────────────────────┐
│ 0..7     │ 8..15    │ 16..35          │ 36..55           │ 56..79               │
│ Lightning│ Blue     │ Fire Bed         │ Warning          │ Candle Wall          │
└──────────┴──────────┴──────────────────┴──────────────────┴──────────────────────┘
```

Studio should validate overlap/range mistakes before a configuration is accepted.

### 3. Cue inspector

A selected pixel cue should expose the logical target plus effect parameters, not raw transport commands.

Example:

```text
Cue name       Lightning hit
Start          00:02.500
Target         lightning-zone
Effect         LIGHTNING
Primary        white
Secondary      black
Brightness     255
Speed          80
Intensity      95
Randomness     90
Duration       900 ms
```

### 4. FX preset library

Studio should allow reusable **looks** without inventing new firmware FX IDs:

```text
Violent White Lightning
Dim Candle
Deep Red Pulse
Broken Fluorescent
Hot Fire
Slow Portal
```

Each preset is one shared FX plus parameters.

### 5. Audio browser

Show/programme audio belongs to the specialist ESP32-A1S/ES8388 Audio Node, not the P4 onboard speaker.

Studio should eventually browse the Node's reported audio inventory and create timeline cues against logical Audio Node targets.

P4 onboard ES8311 remains system/safety audio only.

### 6. Test/commissioning controls

Useful controls include:

```text
TEST CUE
TEST SEGMENT
TEST FROM HERE
STOP
EMERGENCY STOP
```

Testing must still pass through P4 authority/safety gating.

## Production data direction

Production format v1 currently accepts TEST/LOG timeline cues only. The examples below are **target schema**, not a claim that they parse today.

### Persistent pixel line/segment definition target

```json
{
  "pixelLines": [
    {
      "id": "p4.main",
      "device": "p4",
      "pixelCount": 80,
      "segments": [
        {
          "id": "lightning-zone",
          "name": "Lightning Zone",
          "start": 0,
          "count": 8
        },
        {
          "id": "fire-bed",
          "name": "Fire Bed",
          "start": 16,
          "count": 20
        }
      ]
    }
  ]
}
```

### Pixel cue target

```json
{
  "timeMs": 2500,
  "type": "PIXEL",
  "target": "p4.main.lightning-zone",
  "action": "FX",
  "effect": "LIGHTNING",
  "color": [255,255,255],
  "brightness": 255,
  "speed": 80,
  "intensity": 95,
  "randomness": 90,
  "durationMs": 900
}
```

### Audio cue target

```json
{
  "timeMs": 0,
  "type": "AUDIO",
  "target": "audio.main",
  "action": "LOOP",
  "file": "ambience/chamber.wav",
  "volume": 80
}
```

The P4 should resolve logical targets and issue commands to the correct local engine or specialist Node.

## Emergency design is not editable

Studio may **display** emergency policy but must never allow a show designer to recolour/disable it.

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

This applies across GPIO23, GPIO24 and all future pixel-capable Nodes.

GPIO24 is a designated emergency/signage line, not a normal timeline lane. Each 10-pixel sign group normally shows one green locator pixel and nine off pixels; emergency forces all pixels white in sync.

Emergency clear does not auto-resume interrupted FX/show playback.

## First real-show milestone

A practical first Scene Creator/runtime milestone is a short stored production that can eventually perform something like:

```text
00:00  Audio Node ambience LOOP
00:02  warm-glow segment SOLID/FADE
00:05  lightning-zone LIGHTNING
00:05  Audio Node thunder PLAY
00:08  pixel segments FADE_OUT / BLACKOUT
00:10  Audio Node STOP/FADE
00:10  SHOW COMPLETE
```

Today the local pixel engine and Audio Node command/lifecycle foundations exist, but production-format AUDIO/PIXEL cue parsing/dispatch still needs to be connected before this example is a real stored production.

## Safety/independence requirement

Once the P4 has loaded and started a show, the performance must continue without:

- the Director;
- an open Studio browser;
- an associated Wi-Fi client;
- internet.

The P4 owns runtime execution. Studio is an authoring/operator surface, not the show clock.

## Final design statement

Showduino is not a relay controller with a web page bolted on.

It is a show-creation/runtime system in which audio, segmented pixels, timing, specialist Nodes and safety behavior are authored as a coherent performance while the P4 remains authoritative.
