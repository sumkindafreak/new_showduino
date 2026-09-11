# Showduino Studio — Pixel Segment Authoring

Status: **segment-first authoring baseline; live commissioning editor implemented in Studio source**.

## Core rule

Studio authors **pixel segments**, not individual LEDs and not raw animation frames.

A physical line is divided into named regions. Each region then receives its own effect and parameters.

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

```text
physical line
    ↓ split once
named segments
    ↓
FX + colour + speed + intensity + timing
    ↓
show cues
```

This lets Studio present theatrical cues such as:

```text
00:02.000  LIGHTNING ZONE → LIGHTNING
00:02.000  BLUE ALCOVE    → SOLID BLUE
00:04.500  FIRE BED       → FIRE
00:07.000  CANDLE WALL    → FADE_OUT
```

instead of exposing low-level LED updates.

## Current live commissioning editor

`web/showduino-studio/js/pages/Outputs.js` now contains a live GPIO23 segment commissioning/editor surface.

It exposes:

- segment slot 0-15;
- start pixel and pixel count;
- the shared 25-FX vocabulary;
- primary and secondary colours;
- segment brightness;
- speed;
- intensity;
- randomness;
- reverse/direction flag;
- duration;
- apply, apply+start, start, stop and status controls;
- global GPIO23 brightness, blackout and test controls.

The browser UI is hosted by the **dedicated S3 Communications Engine**. It is a client/transport surface only:

```text
Browser
  → S3 SoftAP + Studio frontend
  → S3 API proxy / UART
  → P4 authoritative pixel engine
```

The P4 remains the final validator and owner of pixel state. Studio controls are locked during emergency.

### Important S3 embed step

The canonical frontend source is `web/showduino-studio/`, but the S3 serves a generated PROGMEM bundle:

```text
firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h
```

After Studio source changes, regenerate that bundle with `tools/embed-webui/embed_webui.py` before expecting the changed UI to appear in a newly flashed S3 image. The source and generated bundle must not silently drift.

## Segment definition target

Each persistent segment should eventually include at least:

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

This remains a design target, **not** production-format-v1 data yet.

## Full authoring UI direction

The eventual production designer should show a physical line as a strip divided into named blocks:

```text
MAIN SHOW PIXELS — 0..79

┌──────────┬──────────┬──────────────────┬──────────────────┬──────────────────────┐
│ 0..7     │ 8..15    │ 16..35          │ 36..55           │ 56..79               │
│ Lightning│ Blue     │ Fire Bed         │ Warning          │ Candle Wall          │
└──────────┴──────────┴──────────────────┴──────────────────┴──────────────────────┘
```

Selecting a segment exposes its FX controls. The commissioning editor already proves the control vocabulary; the next authoring step is persistent **named** segment definitions and timeline integration.

## Cue model

A production cue should target the logical segment rather than repeat the physical range every time:

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

The P4 will resolve the logical target to its physical start/count. Production format v1 does **not** accept this cue shape yet; it currently accepts TEST/LOG cues only.

## Reusable FX presets

Studio should later allow reusable named looks such as:

```text
Violent White Lightning
Dim Candle
Deep Red Pulse
Broken Fluorescent
Hot Fire
Slow Portal
```

A preset references a shared Showduino FX plus parameters. It does not create a new firmware effect ID.

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

Studio should not care whether a segment ultimately lives on:

```text
P4 GPIO23 local Show Pixel Line
C3 Pixel Node
future pixel-capable Showduino node
```

All should use the shared vocabulary in:

```text
protocol/showduino_pixel_fx.h
```

The target changes; the authoring experience remains the same.

## Emergency policy in Studio

Emergency behavior is **not editable by a show designer**.

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

This applies to every segment on every pixel-capable output. Normal segment FX are bypassed while emergency is active.

GPIO24 designated emergency/signage pixels are safety-owned and do not appear as a normal editable production lane. During normal operation each 10-pixel sign group shows one green locator pixel and nine off pixels; in emergency the entire line becomes synchronized bright white.

Emergency clear does not auto-resume interrupted show FX.

## Current implementation boundary

Implemented now:

- GPIO23 segmented engine;
- C3 Pixel Node commissioning/editor cards (same model);
- up to 16 live segment slots;
- direct segment commands;
- shared 25-FX vocabulary;
- hard global emergency-white override on P4 local pixels and Pixel Nodes;
- GPIO24 grouped green-locator / synchronized-white signage behavior;
- Studio Outputs-page commissioning/editor controls in source.

Still required for a complete production authoring workflow:

- regeneration of the S3 PROGMEM bundle after Studio source changes;
- persistent named segment definitions in production/project data;
- reusable FX preset persistence/UI;
- website Studio V4 Pixel Node device picker parity;
- logical segment-target resolution;
- reuse of additional Pixel Nodes in the field.
- authoritative cue/result state surfaces for production playback;
- reuse of the same segment/FX model on the future C3 Pixel Node.

Until production PIXEL cues are implemented, direct `PIXEL:SEGMENT:...` commands remain the commissioning/runtime-control interface.
