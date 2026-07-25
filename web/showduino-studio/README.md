# Showduino Studio WebUI

Production browser operating console served by the Communications Engine (**SUE / ESP32-C3**).

This UI follows the same concepts as the Director desk:

- runtime
- emergency/safety
- nodes
- shows
- lighting
- audio
- assets
- networking

## Runtime architecture

```text
Browser shell (single-page operating console)
   ├─ Persistent application shell (nav + header + status surface)
   ├─ Shared runtime store (single authority)
   │   ├─ REST refresh scheduler
   │   ├─ WebSocket ingest
   │   ├─ reconnect/backoff lifecycle
   │   ├─ node freshness computation
   │   └─ notifications + diagnostics
   └─ Route child views (Dashboard, Live, Nodes, ...)

SUE / C3
   ├─ serves embedded static assets
   ├─ emits realtime WebSocket events
   └─ proxies `/api/system` and `/api/logs` to P4 via UART tunnel

P4 / Show Engine
   └─ authoritative runtime + system API source
```

## State ownership model

`js/state/runtimeStore.js` is the sole owner of application runtime state:

- `systemState`
- `runtimeStatus`
- `networkState`
- `emergencyState`
- `nodeCollection`
- `showCollection`
- `assetCollection`
- `configurationState`
- `notifications`
- `diagnostics`

Pages do not run independent polling loops; every page observes the shared store.

## Connection lifecycle

Single source lifecycle states:

- `connecting`
- `connected`
- `degraded`
- `reconnecting`
- `offline`

Lifecycle transitions are driven by WebSocket state + frame freshness + reconnect backoff, not by page logic.

## Navigation architecture

Navigation is permanently mounted and split into:

### OPERATE

- Dashboard
- Live
- Timeline
- Shows
- Nodes
- Logs
- Diagnostics

### CONFIGURE

- Network
- Lighting
- Audio
- Assets
- Node Configuration
- Settings

Unfinished operational tools are explicitly marked **Coming Soon** instead of exposing incomplete controls.

## Data flow

1. Shared store performs scheduled REST refreshes.
2. Shared store opens WebSocket realtime stream.
3. Incoming updates merge into canonical runtime state.
4. Pages re-render from the same data model.
5. Status surface reflects global state continuously.

## Connect for manual testing

1. Flash **P4**, **C3**, **Director**
2. Join Wi-Fi AP `Showduino-Studio` / `showduino`
3. Open `http://192.168.4.1/`

## API surface currently consumed

- `GET /api/system`
- `GET /api/devices`
- `GET /api/network`
- `GET /api/logs`
- `GET /api/commands`
- `GET /api/time`
- `GET /api/time/status`
- `GET /api/capabilities`
- `GET /api/device-capabilities`
- `GET /api/routes`

## Regenerate embedded assets

From repository root:

```bash
python3 tools/gen_web_studio_assets.py
pwsh -File tools/embed-web-studio-assets.ps1
```