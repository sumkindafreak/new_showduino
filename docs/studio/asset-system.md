# Showduino Studio — Asset System Specification

**Status:** design blueprint aligned to the current P4/Audio/Pixel architecture.

## 1. Purpose

Organise reusable resources a Production references without confusing them with the cue list or physical wiring.

The P4 remains the authoritative production store. The S3-hosted Studio is the authoring/client surface.

## 2. Asset kinds

| Kind | Examples | Notes |
|------|----------|-------|
| `audio` | Audio Node WAV references | Programme audio belongs to specialist Audio Nodes |
| `image` | thumbnails / show artwork | Director UI theme assets remain separate |
| `pixel_fx_preset` | Violent Lightning, Dim Candle, Hot Fire | Shared FX name + parameters; does not invent firmware FX IDs |
| `lighting_look` | reusable collections of segment/preset assignments | Logical targets only |
| `variable` | show variables | Typed; cue-settable when runtime supports it |
| `node_resource` | specialist-node resources/config refs | Bound through logical device IDs |
| `other` | extension blobs | Must validate references |

Reserved/future kinds may include video or other media, but should not be treated as live capability until the corresponding engine exists.

**DMX assets are parked/out of scope.** Existing historical DMX schema notes may remain for future compatibility, but Studio must not prioritise or expose production DMX authoring until that work is explicitly unparked.

## 3. Pixel segments are configuration, FX presets are assets

A useful distinction:

- a **segment** defines *where* pixels are: device/line + start/count + friendly logical ID;
- an **FX preset** defines *how* a segment should look: effect + colours + brightness/speed/intensity/etc.;
- a **cue** defines *when* a logical segment should use a preset/effect.

Example segment target:

```json
{
  "id": "lightning-zone",
  "device": "p4",
  "line": "main",
  "start": 0,
  "count": 8
}
```

Example reusable FX preset:

```json
{
  "id": "violent-white-lightning",
  "kind": "pixel_fx_preset",
  "name": "Violent White Lightning",
  "effect": "LIGHTNING",
  "primary": [255,255,255],
  "secondary": [0,0,0],
  "brightness": 255,
  "speed": 82,
  "intensity": 95,
  "randomness": 90
}
```

The canonical FX vocabulary is shared by P4 and future C3 Pixel firmware through `protocol/showduino_pixel_fx.h`.

## 4. Current 25-FX vocabulary

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

Presets reference these names plus parameters. They do not add new runtime enum values.

## 5. Organisation target

```text
assets/
  <kind>/
    <asset-id>/
      asset.json
      payload...
```

`asset.json` minimum target:

| Field | Required |
|-------|----------|
| `id` | Yes |
| `kind` | Yes |
| `name` | Yes |
| `version` | Recommended |
| `tags` / `categories` | Optional |
| `hash` | Required on deployed package where payload exists |
| `refs` | Outbound references to other asset IDs |

Production-format-v1 does **not** implement this full asset system yet. It currently provides the production manifest/timeline foundation and TEST/LOG cue loading.

## 6. Audio assets

Programme audio belongs to the specialist ESP32-A1S/ES8388 Audio Node and its local SD library.

Studio should store/reference logical asset identity and target Audio Node, while deployment/validation ensures the required Node-local WAV exists.

P4 onboard ES8311 system sounds are system/safety resources and are **not** ordinary Production audio assets.

## 7. Catalogue features

| Feature | Behaviour |
|---------|-----------|
| Search | name, ID, tag, kind, unused flag |
| Categories | folders/tags, not a second identity |
| Preview | kind-appropriate preview |
| Unused assets | warning by default |
| Broken references | blocking validation error |
| Pixel preset preview | approximate visual preview only; P4 remains execution authority |

## 8. Import/deploy rules

- Authoring may reference local workspace files temporarily.
- Deployment must resolve/copy required payloads to their authoritative destination or fail validation.
- No deployed production should silently depend on a desktop absolute path.
- Hash/integrity metadata should be recorded where payload deployment is supported.
- Node-local assets must validate against the target Node inventory when that deployment workflow is implemented.

## 9. Emergency behavior is not an asset

The global pixel emergency rule is **not configurable as a preset or Production asset**:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

GPIO24 emergency/signage behavior is safety-owned and must not be represented as an editable lighting asset. Normal per 10-pixel sign is one green locator plus nine off; emergency is ten white.

No Production package may override this policy.

## 10. Relationship to Director UI assets

Director `/showduino/ui/...` themes are not Production assets. Studio may manage them later as a separate Director-skin workflow, but they do not belong in the show’s pixel/audio asset model.

## 11. Variables

Variables remain a planned first-class resource:

| Field | Notes |
|-------|-------|
| `id`, `name` | logical identity |
| `type` | bool, int, string initially |
| `default` | initial value |
| `persist` | production/session policy when implemented |

Cues may eventually set variables and triggers may condition on them. Dependency validation must reject invalid/circular definitions.

## 12. Current implementation boundary

Implemented now:

- P4 production storage foundation;
- specialist Audio Node inventory/control foundation;
- P4 GPIO23 segmented pixel engine;
- shared pixel FX vocabulary;
- Studio Outputs-page segment commissioning/editor source.

Still future:

- persistent named segment definitions;
- `pixel_fx_preset` persistence/catalogue;
- production AUDIO/PIXEL cue parsing;
- deployment of Audio Node assets from Studio;
- logical target resolution end to end.

## Related

- [Pixel Segment Authoring](../studio-pixel-authoring.md)
- [Cue System](cue-system.md)
- [Validation](validation.md)
- [Production Format](production-format.md)
