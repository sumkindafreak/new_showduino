# Showduino WebUI Production Shell (Phase 1)

## Purpose

Phase 1 transitions the browser UI from page-local logic into a single operating-console shell with persistent shared state.

The objective is Director-consistent operator behaviour, not disconnected website pages.

## Shell ownership

The shell remains mounted for the lifetime of the session and owns:

- connection lifecycle
- runtime state
- emergency state
- current show context
- selected node
- notifications
- navigation
- status surface
- responsive layout

Only child route views swap inside `#page-content`.

## Navigation model

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

Pages that are not production-ready are either hidden from nav or rendered as explicit `Coming Soon`.

## Shared runtime model

`web/showduino-studio/js/state/runtimeStore.js` is the single runtime authority.

Primary sections:

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

## State flow

1. Store starts periodic REST refreshers.
2. Store starts WebSocket stream.
3. Incoming messages merge into canonical state.
4. Store computes node freshness and connection health.
5. Pages subscribe and re-render from the same state snapshot.

No route file owns independent transport health checks or polling loops.

## Connection lifecycle contract

Single source values:

- `connecting`
- `connected`
- `degraded`
- `reconnecting`
- `offline`

Transitions are driven by WebSocket connect/close + frame freshness watchdog + reconnect backoff.

## Runtime health behaviour

- Node freshness is computed in one place from `lastSeenMs` and network computed time.
- Degraded status appears when realtime frames go stale.
- Offline appears after sustained reconnect timeout.
- Status surface remains visible on every page and reflects current global health.

## Developer guidance

When adding a new page:

1. Add route + nav item in `js/app.js`.
2. Read all runtime data via `subscribeRuntime()`.
3. Do not add page-level polling timers.
4. Do not instantiate additional WebSocket clients.
5. Preserve Director terminology and avoid alternate naming.

When adding a new API field:

1. Merge it in `runtimeStore`.
2. Expose the mapped value through existing model sections.
3. Keep derived state calculations in store (not in pages).
4. Update this document and `web/showduino-studio/README.md`.
