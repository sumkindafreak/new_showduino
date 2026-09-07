# Showduino WebUI

Browser configuration, production management, diagnostics, and commissioning front door.

This is **not** the Director, and it is **not** the Show Engine.

```text
Browser
  → Comms S3 SoftAP (PROGMEM static UI)
  → UART WEB/GET or WEB/POST
  → P4 WebApiHandler (authoritative JSON)
```

| Copy | Location |
|------|----------|
| **Canonical source** | `web/showduino-studio/` |
| **Runtime** | `firmware/s3-comms-controller/.../src/web/WebAssets.generated.h` |

```text
python tools/embed-webui/embed_webui.py
```

No Node/npm. No CDN. No SD frontend. Do not hand-edit the generated header.

The embedder inlines the `js/app.js` module graph into one `/js/app.js`. SoftAP then serves three files: `index.html`, `studio.css`, and that bundle. Source files stay as ES modules for editing.

## Role

| Surface | Role |
|---------|------|
| Director | Operator touchscreen |
| Comms S3 | Communications engine and WebUI host |
| P4 | Authoritative Show Engine |

The WebUI may display S3 transport (`GET /api/comms`) and proxy P4 APIs. It must not invent show, production, emergency, node, output, or E1.31 state.

## Audio and pixel model

Keep these roles distinct on every operator surface:

```text
SYSTEM AUDIO     P4 onboard ES8311 — boot, notifications, faults, emergency
SHOW AUDIO       specialist Audio Node — music, ambience, dialogue, scare SFX
EMERGENCY PIXELS P4 GPIO24 — safety only; never a show-output lane
SHOW PIXELS      one planned P4 show line + future Pixel Nodes
```

ESP-NOW carries commands and state, never PCM. The Audio Node plays local SD assets and reports lifecycle; the P4 remains authoritative. Controls must show P4-confirmed state, not “command was forwarded”. On `EMERGENCY:STOP`, programme audio stops and does not auto-resume.

`#/audio` and `#/lighting` redirect to `#/outputs`.

## Page map

| Route | Page | Status | Purpose |
|-------|------|--------|---------|
| `#/` | Home | IMPLEMENTED | Comms / P4 / Director / system summary |
| `#/productions` | Productions | IMPLEMENTED | Persistent P4 productions, load/unload |
| `#/live` | Live | IMPLEMENTED | Authoritative runtime, transport, emergency, cues |
| `#/outputs` | Outputs | PARTIAL | Emergency pixels + Audio Node play/loop/fade/duck (attraction audio) |
| `#/devices` | Devices | IMPLEMENTED | Plug-in Bus live; Audio Node card + commissioning details |
| `#/network` | Network | IMPLEMENTED | Comms SoftAP + P4 Ethernet live/saved + E1.31 test monitor |
| `#/system` | System | IMPLEMENTED | S3 / P4 / Director diagnostics, P4 system audio, SD storage, logs |
| `#/settings` | Settings | PARTIAL | Local preferences; AP/Ethernet writes PLANNED |

### Planned (no live controls)

- Production E1.31 engine / TX / cue mapping
- Show Pixel Line engine
- Further Showduino Nodes (Relay / MOSFET / LED / DMX)
- Director Ethernet / E1.31 operator page

### Removed / consolidated

| Old route | Now |
|-----------|-----|
| `#/shows` | Productions |
| `#/commands`, `#/scenes` | Live |
| `#/audio`, `#/lighting` | Outputs |
| `#/logs`, `#/capabilities`, `#/routing`, `#/time` | System |

Old hashes still resolve so bookmarks do not go blank.

## Status words

ONLINE, OFFLINE, READY, DEGRADED, SEARCHING, SYNCHRONISING, RUNNING, PAUSED, STOPPED, EMERGENCY, FAULT, UNCONFIGURED, PLANNED.

The status shell (every page) shows Comms, P4, Director, system health, current production, and emergency.

If P4 drops: the UI stays loaded, **P4 OFFLINE** is shown, and P4-owned controls disable. Show state is not invented.

## Polling

| Source | Interval | Notes |
|--------|----------|-------|
| `GET /api/comms` | 2 s | S3 local, no UART |
| `GET /api/system` | 2 s | Only while Comms reports P4 online |
| Live `#/live` `GET /api/show` | 2 s | Cue stack while that page is open |
| Productions | 4 s | Inventory while that page is open |
| Outputs lighting | 4 s | Emergency pixel line |
| Network P4 view | 4 s | UART-seen counters |
| System logs | 5 s | P4 HTTP ring buffer only |

Hidden pages stop their extra polls.

## Connect

1. Flash **P4**, **S3 Comms Controller**, **Director**.
2. Wire UART (S3 GPIO17→P4 GPIO4, S3 GPIO18←P4 GPIO5).
3. Copy the S3 boot MAC into Director `SHOWDUINO_COMMS_MAC_*`.
4. Join Wi-Fi: `Showduino` / `showduino` (documented bench WPA2 secret).
5. Open `http://192.168.4.1/`

Director SoftAP WebUI is disabled. P4 static SD frontend is compiled out. The old C3 / SUE SoftAP path is not supported.

## APIs

| Endpoint | Owner | Description |
|----------|--------|-------------|
| `GET /api/comms` | S3 | Firmware, MAC, channel, Director/P4 link, RX/TX, WebUI build, status RGB |
| `GET /api/system` | P4 | Runtime, storage, emergency, plugin bus, audio |
| `GET /api/show` | P4 | Timeline playhead and cue list |
| `GET /api/productions` | P4 | SD production inventory |
| `GET /api/devices` | P4 | Show Engine + plugin devices |
| `GET /api/plugins` | P4 | Plug-in Bus |
| `GET /api/audio` | P4 | Stage audio |
| `GET /api/lighting` | P4 | Emergency pixels |
| `GET /api/time` | P4 | Internal RTC |
| `GET /api/capabilities` | P4 | Engine + plugin capabilities |
| `GET /api/network` | P4 | UART / Director-seen |
| `GET /api/logs` | P4 | Web API ring buffer |
| `POST /api/command` | P4 | Whitelisted colon commands only |

`POST /api/command` body: `{"cmd":"SHOW:START"}`. Allowed: `SHOW:START|PAUSE|RESUME|STOP`, `PRODUCTION:LOAD:<id>`, `PRODUCTION:UNLOAD`, `PLUGIN:SCAN`, `AUDIO:STOP`, `EMERGENCY:STOP`, `EMERGENCY:CLEAR_CONFIRM`, `EMERGENCY:CLEAR_CANCEL`, `STATUS:REQUEST`, `TIME:REQUEST`, `TIME:SET:<epoch|ISO>`. USB `EMERGENCY:CLEAR` is not exposed.

**PANIC = EMERGENCY:STOP.**
