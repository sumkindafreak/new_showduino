# Showduino Director — Product UI Architecture

Status: production implementation reference for the post-beta Director UI overhaul.

Related validation artifact: `docs/director-hardware-readiness-checklist.md`

## Implementation status (agent/director-ui-overhaul)

- Phase 1 (permanent shell) is implemented in source:
  - Dock is fixed as `Desktop | Live | Shows | More | E-STOP`
  - `SCREEN:MORE` route is active
  - More launcher exists as a dedicated destination screen
  - Desktop no longer owns transport controls
  - Live owns primary show transport controls
  - E-STOP remains permanently available
- Phase 2 (show workflow) is implemented:
  - Shows is a deliberate package-library browser with scan/storage states and empty-state handling
  - Selection opens Show Details without auto-load or auto-run
  - Show Details presents identity, metadata, requirements, validation, and explicit load status
  - Load flow is request -> awaiting Stage confirmation -> loaded/open Live (without auto-start)
- Phase 3 (nodes + diagnostics separation) is implemented:
  - Nodes is now a dedicated device-health page with grouped roles and an explicit discovery workflow
  - Node Details is a separate view with identity/connection/runtime sections and safe actions only
  - Diagnostics is now a separate technical screen (transport/storage/self-test/telemetry), no longer merged into Nodes
  - More launcher routes Nodes and Diagnostics to different screens
  - Desktop node card remains compact (online count, expected count, health, critical warning)
  - Node UI is honest about current protocol limits: individual CHILDREN/GRANDCHILDREN/SAVIOUR live records are not streamed by Stage yet
- Phase 4 (P4 / IAN audio ownership clarity) is implemented in current production UI language:
  - Director is monitoring/control only for audio commands and mirrored status
  - IAN / P4 remains authoritative for show audio and runtime-linked audio state
  - UI copy no longer describes the Director as an independent audio engine
- Phase 5 (system-screen consolidation) is implemented as a practical production pass:
  - Logs supports pause/resume plus latest jump and clear confirmation
  - Settings routes maintenance and about to dedicated destinations
  - Maintenance is separated from normal settings cards
  - About is informational only and includes build/role identity context
- Phase 6 (hardening pass) is implemented:
  - Emergency overlay now cancels pending destructive confirmations and stays top-priority
  - Shared destructive-action confirmation pattern added for high-impact maintenance actions
  - Navigation consistency updated for Logs/Diagnostics/Maintenance/About paths
  - Runtime update churn reduced in key widgets and terminology consistency tightened

The beta interface proved the ESP32-S3 display, LVGL 9 rendering, GT911 touch input, SD assets, navigation, runtime mirroring, ESP-NOW command paths and emergency handling. This document defines the production information architecture. It is intentionally operator-led: every item has one primary home, live-operation controls remain easy to reach, and diagnostic detail is separated from normal show operation.

## Product principles

1. One clear purpose per screen.
2. One primary home for every piece of information.
3. Desktop is an overview, not a diagnostics console.
4. Live owns playback operation.
5. Emergency is a system-level overlay, never an ordinary page.
6. Primary controls must remain usable quickly, under pressure and in low light.
7. Detailed technical data is progressively disclosed rather than displayed everywhere.
8. The fixed dock must remain stable across the product.
9. Runtime, transport, safety and protocol behaviour must not be changed by visual work.
10. Empty or unavailable data must be presented honestly rather than filled with fake live values.

## Permanent shell

### Status bar

Owns only global fabric information:

- Showduino identity
- Date and time
- Director/Stage link health
- Network/fabric health
- Node count
- Emergency state

It does not own show playback state, cue state or detailed diagnostics.

### Primary dock

Production target:

- Desktop
- Live
- Shows
- More
- E-STOP

`More` opens a launcher for Nodes, Audio, Logs, Settings, Diagnostics and Maintenance. About is reached from Settings. The launcher is implemented and serves as the primary path to secondary destinations so the dock remains uncluttered.

### Emergency control

E-STOP remains permanently visible. The emergency overlay is rendered above the current screen and blocks ordinary interaction. Resume is never automatic.

## Screen ownership

## 1. Desktop

Purpose: calm operational overview and the normal landing screen.

Contains:

- Current show
- Runtime state
- Overall system/fabric health
- Stage connection
- Online node count
- Safety state
- Current time and uptime where useful
- Show progress when a show is loaded or running
- Compact audio health summary
- Latest important operator events
- Workflow actions: Open Live, Browse Shows, View Nodes, Open Audio

Does not contain:

- Start, Pause, Resume or Stop transport controls
- Full node lists
- Full event history
- Raw packet counters as a headline item
- Audio mixer controls
- Configuration controls

Desktop answers: “Is Showduino healthy, what is loaded, and where should I go next?”

## 2. Live

Purpose: primary operator surface while a show is running.

Contains:

- Current show
- Runtime state
- Current cue and next cue
- Cue index and cue total
- Elapsed and remaining time
- Large show progress indicator
- Stage connection and compact warning state
- Start, Pause, Resume and Stop
- Skip/previous cue when supported safely
- Manual trigger controls
- Relay/manual output controls when required by the loaded show

Does not contain:

- Show library
- Full node diagnostics
- Settings
- Long logs
- Show editing

Live answers: “What is happening now, what happens next, and what control do I need?”

## 3. Shows

Purpose: browse and select show packages.

Contains:

- Show artwork or generated thumbnail
- Name
- Duration
- Cue count
- Validation/compatibility state
- Last modified information
- Refresh/rescan action
- Open details action

Selecting a show opens Show Details. It does not immediately start playback.

## 4. Show Details

Purpose: inspect and deliberately load one selected show.

Contains:

- Artwork
- Title and description
- Duration and cue count
- Required nodes
- Required audio assets
- Lighting/fixture requirements
- Validation results and missing-asset warnings
- Load Show
- View cue list
- Back to Shows

Playback transport belongs to Live. Show Details does not expose full transport controls and does not start playback directly.

## 5. Nodes

Purpose: network and device health.

Contains:

- Summary strip: expected nodes, online count, offline count, warning/fault count
- Discovery state and last discovery result
- Stage/SUE connection state
- Grouped list in fixed role order:
  - Director
  - SUE
  - IAN
  - CHILDREN
  - GRANDCHILDREN
  - SAVIOUR
- Within-group sorting by severity (Fault, Offline, Warning/Degraded, Online, Unknown) then name/ID
- Readable per-node cards (name, role, ID, state, last seen, connection path)
- Explicit empty states for role groups without reported records
- Safe actions only: Discover/Refresh, Open Diagnostics, Node Details

Selecting a node opens progressively disclosed technical details such as MAC/device ID, IP address, uptime, sensors, outputs, last command and last response.

### Node Details behaviour

- Identity section: name, role, ID, MAC, firmware, hardware/transport
- Connection section: online/offline, last seen, signal (if reported), transport/path, discovery status, last command/response, packet counters
- Runtime section: uptime, active show/runtime state/cue and honest "Not reported" placeholders for unavailable fields
- Actions are non-destructive: refresh status, rediscover, open diagnostics, back to nodes

### Known data limitations (current protocol/runtime)

- Director currently receives reliable aggregate node availability/count and link/runtime mirrors.
- Stage does **not** yet stream a full live per-node registry (role-tagged device records with live RSSI/battery/firmware/uptime for all remote nodes).
- Stored paired-device records are available from Director SD and are shown as historical/known records, not fabricated live telemetry.
- Future protocol requirement: add a compact, backward-compatible Stage node-registry/status payload so Director can show full per-node live health for CHILDREN/GRANDCHILDREN/SAVIOUR without inference.

## 6. Diagnostics

Purpose: technical health checks and protocol/service visibility, separate from normal node operations.

Contains:

- Stage status request
- Stage hello/test request
- Discovery trigger
- SD status
- Self test
- Packet counters, memory snapshots, and last command/response telemetry

Diagnostics is intentionally not the normal show-operation surface.

## 7. Audio

Purpose: Director monitoring/control surface for the IAN/P4 audio engine.

Contains:

- P4 / IAN reported playback state
- Current track/asset information where reported
- Master command controls (play/pause/stop/mute/volume requests)
- Output connection and readiness indicators where reported
- Remote-node command path status where available
- Explicit pending/awaiting-status messaging for command confirmation

The Director does not decode or output show audio locally; IAN/P4 remains authoritative.

## 8. Logs

Purpose: operator and diagnostic history.

Contains:

- Timestamp
- Severity
- Source
- Event message
- Filters: All, System, Show, Audio, Network, Emergency
- Pause/resume live updates
- Latest jump while paused/active
- Clear and export actions
- Scrollable history

Desktop shows only a compact recent-events summary.

## 9. Settings

Purpose: configuration and maintenance, never routine show operation.

Sections:

### System

- Director name
- Brightness
- Date/time
- Restart
- Software version
- Storage information

### Network

- Wi-Fi
- Showduino fabric
- SUE connection
- ESP-NOW status
- Discovery options

### Show operation

- Confirmation policy
- Auto-load policy
- Resume behaviour
- Emergency clearing policy

### Display

- Brightness
- Screen timeout
- Touch calibration
- Animation level
- Sound feedback

### Maintenance

- Open Maintenance screen
- Open Diagnostics screen
- About and product information access

Dangerous actions require confirmation and must not appear on the first settings view.

## 10. Maintenance

Purpose: service operations and recovery tools with explicit confirmation requirements.

Contains:

- SD status
- Backup
- Export diagnostics package
- Repair directories (confirmed action)
- Links to Logs and Diagnostics

Unsafe operations are blocked during emergency and while show playback is active.

## 11. About

Purpose: product identity and role ownership reference.

Contains:

- Showduino Director identity
- Firmware version and board target
- LVGL version
- Role ownership summary (Director UI vs IAN/P4 runtime/audio authority)
- Repository/license reference

No destructive controls are exposed on About.

## 12. More launcher

Purpose: keep the permanent dock uncluttered.

Contains large destinations for:

- Nodes
- Audio
- Logs
- Settings
- Diagnostics
- Maintenance

Nodes and Diagnostics are separate destinations and must not share the same screen route.

The launcher contains navigation only, not duplicate status cards.

## 13. Emergency overlay

Purpose: unmistakable system-level safety state with highest interaction priority.

Contains:

- EMERGENCY STOP ACTIVE
- Show name
- Source/reason (or explicit "not reported")
- State before emergency
- Current cue
- Elapsed and remaining time
- Stage connection
- Audio emergency state
- Safe-state confirmation status
- Activation time
- Acknowledge
- Diagnostics
- Clear-state guidance

When emergency activates, pending destructive confirmation dialogs are cancelled and must be re-confirmed after clear.

After Stage confirms the emergency has cleared during playback:

- Resume Show
- Abort Show

After clearing while idle, return to the previous safe page. Never resume automatically.

## Information ownership matrix

| Information | Primary home |
|---|---|
| Overall health | Desktop |
| Current playback | Live |
| Available packages | Shows |
| Selected package requirements | Show Details |
| Device connectivity | Nodes |
| Audio monitoring/control (IAN/P4 authoritative) | Audio |
| Configuration | Settings |
| Service operations | Maintenance |
| Product identity/build info | About |
| Historical events | Logs |
| Emergency condition | Emergency overlay |
| Time and basic fabric health | Status bar |

## Implementation order

1. Shared theme, geometry and scrolling foundation
2. Desktop
3. Permanent dock and More launcher
4. Live
5. Shows
6. Show Details
7. Nodes
8. Audio
9. Logs
10. Settings
11. Emergency overlay visual pass
12. Consistency, memory, compilation and touchscreen testing

## Desktop acceptance criteria

The first production Desktop pass is complete when:

- It contains no transport controls.
- Current show and runtime state are visually dominant.
- Fabric, stage, nodes, safety and audio are visible without becoming raw diagnostics.
- Show progress appears only when meaningful.
- Four large workflow actions lead to Live, Shows, Nodes and Audio.
- Recent events are compact and do not replace the full Logs page.
- No data is duplicated from the status bar without adding operational value.
- All existing command callbacks and emergency behaviour remain intact.
- The screen fits 800×480 with touch targets suitable for real operation.

## Live acceptance criteria

The first production Live pass is complete when:

- Current cue, next cue and progress are dominant.
- Transport controls are large and state-aware.
- Manual outputs are secondary to show playback.
- Emergency remains permanently accessible.
- No library, settings or full diagnostics content is present.
- Existing runtime mirrors and commands remain compatible.

---

## Phase 5 implementation notes (completed)

### Ownership summary

| Screen | Owns |
|--------|------|
| Logs | Operator history, filters (8 categories), pause/resume, clear (confirmed), export |
| Settings | Display prefs (timeout + brightness), navigation hub, system identity |
| Maintenance | Storage ops, backup/export, repair (confirmed), factory reset (confirmed) |
| About | Product identity, firmware/protocol/LVGL version, architecture summary |
| Diagnostics | Technical tools, transport status (Phase 3), P4 audio diagnostics (Phase 4) |

### Logs improvements

- **System filter fixed**: now excludes audio/emergency/show entries rather than passing all
- **Added Warnings (6) and Errors (7)** filter buttons
- Clear Logs uses `showActionConfirm()` modal before clearing
- Export forwarded to `.ino` `exportDiagnostics()` (writes diagnostics JSON to SD)
- Pause shows live/paused state in filter label
- `logsPausedUnseen_` counter tracks new entries while paused

### Settings rebuild

Two-row Display section: timeout presets + brightness ▲▼ buttons (0-255, persists immediately).
System identity: director name readout.
Navigation section: Audio, Logs, Nodes, Diagnostics, Maintenance, About, E-Stop Clear.

### Maintenance enhancements

Existing `buildMaintenancePage()` enhanced with:
- Last operation result label (`maintStatusLabel_`) updated by all STORAGE:* handlers
- Factory Reset button → confirmation dialog → `MAINTENANCE:FACTORY:RESET` → resets config, saves, updates backlight and About/Settings labels
- Clear documentation of safe vs confirmed actions

### About enhancements

`buildAboutPage()` enhanced with:
- `aboutNameLabel_` for the director name (updated from config at startup and after reset)
- Protocol version (`SHOWDUINO_PROTOCOL_VERSION_MAJOR.MINOR`)
- LVGL version (major.minor only — `LVGL_VERSION_MAJOR.MINOR`)

### Public API additions

- `setMaintenanceStatus(msg)` — set Maintenance page last-op label from .ino
- `setAboutDirectorName(name)` — update About + Settings identity labels
- `refreshSettingsDisplayValues_()` — refresh brightness readout

### Settings persistence

- Screen timeout: persists to `DirectorConfig.screenTimeoutMinutes` immediately
- Brightness: persists to `DirectorConfig.brightness` immediately, applies via `backlightConfigure()`
- Both use `markConfigDirty()` + `saveAllConfiguration()` (SD atomic JSON write)
- Factory reset: calls `configManager().resetToDefaults()` + full save

### Emergency interaction

- `hideActionConfirm()` called on emergency activation (already in remote since dc3bb1c)
- Confirmation dialog cannot execute during emergency (showActionConfirm checks `emergencyLocked`)
- `restorePageAfterEmergency()` handles About (stay) and Maintenance (→ Settings)

### Build results

Phase 5 on Phase 3+4+dc3bb1c+dc79a8f base:
- 1401058 bytes (44%) flash
- 141244 bytes (43%) SRAM
- No C++ errors

### Known limitations

- Director name editing via UI not implemented (read-only display)
- Network configuration section not present (ESP-NOW peer is compile-time)
- Log export writes diagnostics JSON, not a plain-text log
- P4 firmware version not shown on About (not reported by P4)
- Hardware testing on physical Director still required

---

## Phase 6 implementation notes (completed)

### Summary

Phase 6 is a targeted hardening pass — no new feature areas added. All changes are defensive and correctness-focused.

### Critical: Duplicate page builder memory leak fixed

`buildAboutPage()` and `buildMaintenancePage()` were called twice in `buildScreens()` — once before Settings, once after. Both builders lacked guards, so the second call created new LVGL screens, orphaned the first pair, and overwrote the member pointers (`aboutScreen`, `maintenanceScreen`, `maintStatusLabel_`, etc.).

**Fix:** Added `if (aboutScreen) return;` / `if (maintenanceScreen) return;` guards. The second call site now becomes a no-op. This prevents LVGL heap waste on every boot.

### Emergency overlay hardening

**EMERGENCY:CLEAR debounce:** The Clear button now checks `pendingClearAwait_` before sending. If a Clear is already in flight, the operator sees "Clear already requested — awaiting Stage" and no duplicate command is sent. `pendingClearRequestMs_` tracks when the Clear was sent. After 12 seconds without Stage response, `pendingClearAwait_` resets, allowing a retry. This timeout resets again when emergency actually clears.

**Safe-state subtitle:** Updated static subtitle in the overlay to read "Stage halted. Clear the emergency, then Resume show or Abort. All outputs are in safe-state pending Stage confirmation."

### Navigation: More launcher

About button re-added to the More launcher alongside Diagnostics and Maintenance (all three fit at reduced width: 140px per button). More now routes to: Nodes, Audio, Logs, Settings, Diagnostics, Maintenance, About.

### UI:COMPLETE:EXPORT stub

The "Export Log — coming soon" placeholder in the Show Complete overlay is now wired to `commandCallback("UI:LOGS:EXPORT")`, which calls real `exportDiagnostics()` in the .ino.

### Code quality

- `logsFilter_` comment corrected from "0=All placeholder" to the full filter index description
- Emergency and P4 audio ownership unchanged

### Build results

- 1,401,442 bytes (44%) flash — +384 bytes over Phase 5 baseline
- 141,252 bytes (43%) SRAM — +8 bytes over Phase 5 baseline
- No C++ errors; only standard linker warnings from pre-built ESP32 library objects

### Static validation: all clean

- E-STOP present on every screen (via `createDock()` → `makeDock()` which always appends E-STOP)
- No LVGL 8 APIs (`lv_obj_del`, `lv_btn_create`, etc.) found
- All 13 DeskPage values handled in `restorePageAfterEmergency()`
- Emergency overlay on `lv_layer_top()` with `lv_obj_move_foreground()` in `showEmergencyOverlay()`
- `hideActionConfirm()` called in 7 locations before emergency overlay appears
- Log display uses single label (`lv_label_set_text`) — no LVGL object growth on refresh
- Node and show card lists call `lv_obj_clean()` before rebuild

### Hardware validation still required

All items in `docs/director-hardware-readiness-checklist.md` require physical Director hardware. No hardware tests were run.
