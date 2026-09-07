# Showduino Studio — Pixel Segment Authoring

Status: design baseline for the Studio show designer.

## Core rule

Studio authors **pixel segments**, not individual LEDs and not raw animation frames.

A physical line is first divided into named regions. Each region then receives its own effect and parameters.

Example:

```text
P4 Show Pixel Line — GPIO23 — 80 pixels

Segment 0  LIGHTNING ZONE   pixels 0-7
Segment 1  BLUE ALCOVE      pixels 8-15
Segment 2  FIRE BED         pixels 16-35
Segment 3  WARNING STRIP    pixels 36-55
Segment 4  CANDLE WALL      pixels 56-79
```

During show design, the operator should work with those named regions rather than repeatedly typing pixel indexes.

## Why segments are the Studio primitive

Segments give the author a stable theatrical object:

```text
physical line
    ↓ split once
named segments
    ↓
FX + colour + speed + intensity + timing
    ↓
show cues
```

This means the Studio can present cues such as:

```text
00:02.000  LIGHTNING ZONE → LIGHTNING
00:02.000  BLUE ALCOVE    → SOLID BLUE
00:04.500  FIRE BED       → FIRE
00:07.000  CANDLE WALL    → FADE_OUT
```

instead of exposing low-level LED updates.

## Segment definition

Each segment should eventually persist with at least:

```text
id
friendly name
line/device target
start pixel
pixel count
default brightness
optional default colour/effect
```

Conceptual project data:

```json
{
  "pixelLines": [
    {
      "id": "p4.main",
      "name": "Main Show Pixels",
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

This is a design target, not the current production-format-v1 schema.

## Studio editor concept

A pixel line should be shown visually as a horizontal strip divided into coloured/labelled segment blocks:

```text
MAIN SHOW PIXELS — 0..79

┌──────────┬──────────┬──────────────────┬──────────────────┬──────────────────────┐
│ 0..7     │ 8..15    │ 16..35           │ 36..55           │ 56..79               │
│ Lightning│ Blue     │ Fire Bed         │ Warning          │ Candle Wall          │
└──────────┴──────────┴──────────────────┴──────────────────┴──────────────────────┘
```

Selecting a segment opens its FX controls:

```text
Effect       LIGHTNING
Colour       255,255,255
Colour 2     0,0,0
Brightness   255
Speed        70
Intensity    85
Randomness   90
Reverse      No
Duration     until next cue / explicit duration
```

## Cue model

A Studio timeline cue should target the logical segment, not repeat the physical range every time.

Preferred authoring shape:

```json
{
  "timeMs": 2500,
  "type": "PIXEL",
  "target": "p4.main.lightning-zone",
  "action": "FX",
  "effect": "LIGHTNING",
  "color": [255, 255, 255],
  "brightness": 255,
  "speed": 70,
  "intensity": 85,
  "randomness": 90
}
```

The P4 resolves the logical segment to its physical start/count before running the effect.

## Reusable FX presets

Studio should later allow an effect configuration to be saved as a reusable preset, for example:

```text
Violent White Lightning
Dim Candle
Deep Red Pulse
Broken Fluorescent
Hot Fire
Slow Portal
```

A preset references one of the shared Showduino FX names plus parameters; it does not create a new firmware effect ID.

Example:

```json
{
  "name": "Violent White Lightning",
  "effect": "LIGHTNING",
  "color": [255,255,255],
  "brightness": 255,
  "speed": 82,
  "intensity": 95,
  "randomness": 90
}
```

## Shared P4 / C3 behaviour

The Studio should not care whether a segment lives on:

```text
P4 GPIO23 local Show Pixel Line
C3 Pixel Node
future pixel-capable Showduino node
```

All use the shared vocabulary in:

```text
protocol/showduino_pixel_fx.h
```

The device target changes; the authoring experience remains the same.

## Emergency policy in Studio

Emergency behaviour is **not editable by a show designer**.

Studio may display the policy, but it must not expose a control that can disable or recolour it:

> EMERGENCY = ALL PIXELS BRIGHT WHITE.

This applies to every segment on every pixel-capable output. Normal segment FX are bypassed while emergency is active.

The GPIO24 designated-signage line is also safety-owned and must not appear as a normal editable production pixel lane.

## Current implementation boundary

Implemented now on the P4:

- GPIO23 segmented engine;
- up to 16 segment slots;
- direct segment commands;
- shared 25-FX vocabulary;
- hard emergency-white override.

Still to implement before this becomes a complete Studio production workflow:

- persistent named segment definitions in production/project data;
- Studio segment editor UI;
- Studio FX preset UI;
- production `PIXEL` cue parsing on the P4;
- logical segment-target resolution;
- confirmed execution/state surfaces in Studio.

Until those are implemented, the direct `PIXEL:SEGMENT:...` commands remain the bench/commissioning interface.
