# Showduino Studio / WebUI

Browser configuration, production management, diagnostics and commissioning front door.

This is **not** the Director and it is **not** the Show Engine.

```text
Browser
  → Communications S3 SoftAP
  → S3 PROGMEM static Showduino Studio
  → S3 API proxy / UART WEB tunnel
  → P4 WebApiHandler
  → authoritative P4 state/engines
```

| Copy | Location |
|------|----------|
| **Canonical editable source** | `web/showduino-studio/` |
| **Generated S3 runtime bundle** | `firmware/s3-comms-controller/ShowduinoS3CommsController/src/web/WebAssets.generated.h` |

Regenerate the S3 bundle after frontend source changes:

```text
python tools/embed-webui/embed_webui.py
```

No Node/npm, no CDN and no P4-SD frontend is required for the canonical S3-hosted UI. Do not hand-edit the generated header.

The embedder inlines the ES-module graph into the S3-served bundle. The source tree remains human-editable ES modules.

## Ownership

| Surface | Role |
|---------|------|
| Director | Touchscreen operator client |
| Communications S3 | ESP-NOW/UART transport + SoftAP + static Studio host |
| P4 | Authoritative Show Engine, API, state, productions and local engines |
| Specialist Nodes | Physical/media execution and result reporting |

The WebUI may display S3-local transport data (`GET /api/comms`) and P4-authoritative API data. It must not invent show, emergency, node, output or production state.

## Current audio/pixel model

```text
SYSTEM AUDIO       P4 onboard ES8311 — Showduino system/safety sounds
SHOW AUDIO         specialist Audio Node — attraction/programme audio
SHOW PIXELS        P4 GPIO23 — one segmented theatrical line
EMERGENCY SIGNAGE  P4 GPIO24 — safety-owned 10-pixel sign groups
FUTURE PIXELS      C3 Pixel Node, after the C3 Lantern Node
```

The current specialist-node order is:

```text
Audio Node
→ C3 Lantern Node
→ C3 Pixel Node
→ MOSFET Node
```

The old Relay Node direction is superseded. DMX remains parked/out of scope.

## Emergency pixel policy

Emergency behavior is fixed and cannot be edited by Studio:

> **EMERGENCY = ALL PIXELS BRIGHT WHITE.**

This overrides every P4 segment/FX and will also apply to all future pixel-capable Nodes.

GPIO24 designated-signage behavior:

```text
NORMAL per 10-pixel sign:  GREEN + 9 OFF
EMERGENCY:                 all 10 WHITE
```

GPIO23 theatrical segments do not auto-resume when emergency is cleared.

## Segment-first pixel design

Segments are the Studio authoring primitive.

A user should define a physical region once and then apply effects to that logical region instead of repeatedly programming individual LED indexes.

Shared FX vocabulary comes from `protocol/showduino_pixel_fx.h` and currently contains 25 entries/aliases including solid, fades, pulse/breathe, flicker/candle/fire, lightning/strobe, chase/comet/wipes/build, sparkle/twinkle/glitch, warning, portal, rainbow and custom sequence.

See [`../../docs/studio-pixel-authoring.md`](../../docs/studio-pixel-authoring.md).

## Outputs page — current source

`js/pages/Outputs.js` now contains a live GPIO23 segment commissioning/editor surface with:

- segment slot 0-15;
- start/count;
- FX picker;
- primary/secondary colours;
- brightness, speed, intensity and randomness;
- reverse and duration;
- apply/start/stop/status;
- global brightness, blackout and pixel-test controls;
- emergency lockout/status display.

The page also displays GPIO24 emergency-signage state and the Audio Node controls.

### Current integration boundary

The Studio source contains the pixel controls, but browser control depends on the P4 Web API allowing and exposing the corresponding `PIXEL:*` command/state surface. Keep the source UI, P4 Web API whitelist/state JSON and generated S3 PROGMEM bundle in sync.

Persistent named segments and production `PIXEL` timeline cues are **not** production-format-v1 features yet. The Outputs-page editor is a commissioning/runtime surface, not a claim that complete production authoring is finished.

## Page map

| Route | Page | Status | Purpose |
|-------|------|--------|---------|
| `#/` | Home | IMPLEMENTED | Comms/P4/Director/system summary |
| `#/productions` | Productions | IMPLEMENTED FOUNDATION | P4 SD production inventory/load/unload |
| `#/live` | Live | IMPLEMENTED FOUNDATION | Authoritative runtime/playhead/cues/emergency |
| `#/outputs` | Outputs | ACTIVE / EXPANDING | Audio Node + GPIO23 segmented pixel commissioning + GPIO24 signage state |
| `#/devices` | Devices | IMPLEMENTED FOUNDATION | Plug-in Bus and Audio Node surfaces |
| `#/network` | Network | IMPLEMENTED / TEST | S3 SoftAP, P4 Ethernet and isolated E1.31 test monitor |
| `#/system` | System | IMPLEMENTED FOUNDATION | S3/P4/Director diagnostics, P4 system audio, SD/logs |
| `#/settings` | Settings | PARTIAL | Local preferences/config surfaces |

Aliases such as `#/audio` and `#/lighting` redirect to Outputs.

## Deliberately not claimed complete

- persistent production AUDIO/PIXEL cue schemas;
- named pixel-segment project persistence;
- reusable FX preset persistence;
- C3 Lantern/C3 Pixel/MOSFET Node commissioning;
- logical device-ID routing end to end;
- generic completion-driven node faults/state;
- DMX/E1.31 production control.

## Status words

Use clear states such as:

```text
ONLINE OFFLINE READY DEGRADED SEARCHING SYNCHRONISING
RUNNING PAUSED STOPPED EMERGENCY FAULT UNCONFIGURED PLANNED
```

If P4 drops offline, the UI remains served by S3 but P4-owned controls disable and state must not be invented.

## Polling

Typical current polling:

| Source | Interval | Notes |
|--------|----------|-------|
| `GET /api/comms` | ~2 s | S3-local, no UART required |
| `GET /api/system` | ~2 s | P4 authoritative system snapshot while P4 reachable |
| `GET /api/show` | ~2 s on Live | runtime/cue surface |
| `GET /api/productions` | ~4 s | production inventory |
| `GET /api/lighting` | ~4 s on Outputs | emergency + show-pixel engine state once API fields are wired |
| `GET /api/network` | ~4 s | P4 network diagnostics |
| `GET /api/logs` | ~5 s | P4 API/log surface |

Hidden pages should stop their extra polling.

## Bench connection

1. Flash the P4, S3 Communications Controller and Director when ready for a hardware test.
2. Wire S3 GPIO17 TX → P4 GPIO4 RX and S3 GPIO18 RX ← P4 GPIO5 TX, plus common GND.
3. Use the S3 boot MAC for the Director `SHOWDUINO_COMMS_MAC_*` peer configuration.
4. Join the S3 `Showduino` SoftAP using the configured bench credentials.
5. Open the S3 WebUI address (current bench default is `192.168.4.1`).

The Director does not host the canonical browser UI. P4 static-SD frontend serving is compiled out in the current architecture.

## APIs

| Endpoint | Owner | Description |
|----------|-------|-------------|
| `GET /api/comms` | S3 | S3 firmware/MAC/channel/link/WebUI build/status |
| `GET /api/system` | P4 | runtime/storage/emergency/plugins/audio/capabilities |
| `GET /api/show` | P4 | timeline playhead and cue list |
| `GET /api/productions` | P4 | SD production inventory |
| `GET /api/devices` | P4 | Show Engine + discovered/configured devices |
| `GET /api/plugins` | P4 | Plug-in Bus |
| `GET /api/audio` | P4 | P4 system/safety audio |
| `GET /api/lighting` | P4 | GPIO24 signage + GPIO23 Show Pixel state once fully wired |
| `GET /api/time` | P4 | internal RTC |
| `GET /api/capabilities` | P4 | engine/plugin capabilities |
| `GET /api/network` | P4 | UART/Ethernet diagnostics |
| `GET /api/logs` | P4 | Web API log ring |
| `POST /api/command` | P4 via S3 | P4-whitelisted commands only |

USB-only emergency maintenance clear is deliberately not exposed through the browser.

**PANIC = EMERGENCY:STOP.**
