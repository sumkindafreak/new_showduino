# Showduino Studio WebUI Architecture

## Overview

Showduino Studio WebUI is a single-page application that acts as the remote console for the Showduino Director. It mirrors the Director's conceptual model and uses identical terminology.

The UI communicates only via:
- **REST API** – initial data load, capability queries, time status
- **WebSocket** – live streaming from the C3 SUE bridge on port 81

No page ever switches a relay, plays audio, or controls LEDs directly. All actions are sent as JSON commands to the firmware. The Brain (Stage Engine) executes them.

---

## Application Shell

The shell (`js/components/Layout.js`) is built once at startup and never reloaded during navigation.

It owns:
- The **Status Bar** – persistent connection/runtime/emergency surface
- The **Sidebar Navigation** – OPERATE and CONFIGURE sections
- The **Page Container** (`#page-content`) – where page views mount

Pages mount as child views inside the container. Navigation swaps the view without rebuilding the shell or resetting shared state.

### Shell structure

```
#app
├── .status-bar          ← always visible
├── .layout
│   ├── .sidebar         ← navigation (OPERATE + CONFIGURE)
│   └── .main
│       ├── .topbar      ← page title + mobile menu toggle
│       └── #page-content  ← page view mounts here
├── .overlay             ← mobile drawer backdrop
└── #toast-container     ← transient notifications
```

---

## Shared Runtime State

`js/state/runtime.js` is the single authoritative state model.

All pages observe this model. No page maintains its own disconnected runtime logic.

### State shape

| Sub-state             | Contents |
|-----------------------|----------|
| `connection`          | One of: `connecting`, `connected`, `degraded`, `reconnecting`, `offline` |
| `system`              | Board name, firmware version, uptime, CPU, memory, storage |
| `runtime`             | Current show, state (`idle`/`running`/`paused`/`stopped`), position, duration, cues |
| `emergency`           | `active`, `reason`, `level` (`null`/`warning`/`critical`) |
| `network`             | Device count, health, RSSI, heartbeat rate, latency |
| `nodes`               | Live device/node list (from WebSocket snapshots and REST) |
| `shows`               | Show library loaded from `/api/shows` |
| `commands`            | Queue, running, history, depth counters |
| `time` / `timeStatus` | Authoritative clock from SUE DS3231 |
| `notifications`       | In-memory notification queue (drives Toast container) |

### Subscribing from a page

```js
import { subscribe } from '../state/runtime.js';

export function MyPage(container) {
  const unsub = subscribe((state) => {
    // re-render from state
  });
  return () => unsub(); // cleanup
}
```

### Mutating state

State is only mutated through the exported mutation functions:

```js
applySystemData(sys)
applyRuntimeData(data)
applyNetworkData(net)
applyEmergencyData(data)
upsertNode(device)
setNodes(nodes)
upsertCommand(cmd)
applyTimeData(time, status)
addNotification(message, level, ttlMs)
```

Never mutate `_state` directly.

---

## Connection Lifecycle

Managed by `js/live.js`, which owns the single WebSocket instance.

```
CONNECTING  →  CONNECTED  ←→  DEGRADED
                    ↓
              RECONNECTING  →  CONNECTED
                    ↓
                OFFLINE
```

### States

| State          | Meaning |
|----------------|---------|
| `connecting`   | Initial connection attempt |
| `connected`    | WebSocket open and receiving messages |
| `degraded`     | Connected but no message for 15 seconds |
| `reconnecting` | Previous connection lost; waiting before retry |
| `offline`      | Explicitly disconnected or max retries exceeded |

### Reconnect backoff

Delay steps: 1s → 2s → 4s → 8s → 16s → 30s (then stays at 30s)

### Degraded detection

If no WebSocket message is received for 15 seconds while in `connected` state, the connection transitions to `degraded`. Any incoming message immediately restores to `connected`.

### Backward compatibility

`live.js` re-exports `getLiveSnapshot` and `subscribeLive` from `state/runtime.js` for any page that still imports from the old location.

---

## Navigation Architecture

### OPERATE section
Pages used during a live show:

| Route          | Page          | Purpose |
|----------------|---------------|---------|
| `/dashboard`   | Dashboard     | Operational overview — system, runtime, nodes, events |
| `/live`        | Live          | Show playback progress, cues, node activity, warnings |
| `/timeline`    | Timeline      | Timeline editor (Phase 2 — Coming Soon) |
| `/shows`       | Shows         | Show library browser |
| `/nodes`       | Nodes         | Node/device inventory and health |
| `/logs`        | Logs          | API log ring buffer |
| `/diagnostics` | Diagnostics   | Commands, capabilities, routing, time |

### CONFIGURE section
System configuration pages:

| Route          | Page          | Purpose |
|----------------|---------------|---------|
| `/network`     | Network       | Wi-Fi and mesh topology |
| `/lighting`    | Lighting      | Relay/LED configuration (Coming Soon) |
| `/audio`       | Audio         | MP3 deck configuration (Coming Soon) |
| `/assets`      | Assets        | Show asset management (Coming Soon) |
| `/nodeconfig`  | Node Config   | Per-node configuration (Coming Soon) |
| `/settings`    | Settings      | System info and firmware identity |

### Coming Soon policy

Incomplete features are always visible in the navigation but clearly marked with a `Soon` badge and non-interactive. Pages show an explanatory Coming Soon screen rather than being hidden. This makes the planned feature surface clear to operators.

---

## Data Flow

```
Firmware (C3 SUE)
    │
    ├── WebSocket :81  →  live.js  →  runtime.js  →  listeners (all pages)
    │
    └── REST API       →  api.js   →  pages fetch on mount
                                       pages call applyXxx() mutation
                                       runtime.js  →  listeners
```

### Polling policy

- **System info** (`/api/system`): fetched on Dashboard mount, refreshed every 10 seconds
- **Network / nodes**: seeded from REST on Nodes/Network mount, then live from WebSocket
- **Logs**: polled every 2 seconds on the Logs page only
- **Everything else**: driven by WebSocket events — no per-page timers

---

## Status Bar

The persistent status bar (`js/components/StatusBar.js`) is always visible, regardless of current page.

It shows:
- **Branding** – SHOWDUINO Studio
- **Connection state** – chip with colour-coded state
- **Runtime state** – IDLE / RUNNING / PAUSED / STOPPED / ERROR
- **Emergency state** – SAFE / ⚠ WARNING / ⚠ EMERGENCY (blinking when active)
- **Node count** – `online/total nodes`
- **Current show** – name of loaded show
- **Clock** – device time if available, local clock otherwise
- **Notification count** – bell icon with badge

---

## API Client

`js/api.js` provides all fetch wrappers with:
- **AbortController** timeout (default 8 seconds per request)
- External signal support (callers can cancel)
- Consistent error formatting

---

## Developer Notes

### Adding a new page

1. Create `js/pages/MyPage.js`
2. Export a function `MyPage(container)` that returns a cleanup function
3. Assign `MyPage.title = 'My Page'`
4. Import from `js/app.js` and call `registerRoute('/mypage', MyPage)`
5. Add to the appropriate `Nav.js` section

### State ownership

- **Runtime state** is owned by `state/runtime.js`. Never duplicate it in a page.
- **Page-local state** (form field values, selected tabs) belongs in the page closure.
- **Connection state** is owned by `live.js`. Never create a second WebSocket.

### No circular dependencies

```
state/runtime.js  ←  live.js  ←  app.js
       ↑                 ↑
   pages/*         components/*
```

`state/runtime.js` has no imports. `live.js` imports from `state/runtime.js` only.

---

## Embedded Assets

The WebUI is embedded into firmware using `tools/gen_web_studio_assets.py`.

```bash
python3 tools/gen_web_studio_assets.py
```

This reads all files from `web/showduino-studio/` and writes them as byte arrays into:
- `firmware/director-esp32-8048s050/ShowduinoDirector8048S050/src/WebStudioAssets.h`

The C3 bridge has its own copy at:
- `firmware/c3-supermini-espnow-bridge/ShowduinoC3SuperMiniBridge/src/WebStudioAssets.h`

Run the generator after any change to web source files.
