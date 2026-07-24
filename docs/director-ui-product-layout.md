# Showduino Director — Product UI Architecture

Status: implementation baseline for the post-beta Director UI overhaul.

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

`More` opens a launcher for Nodes, Audio, Logs, Settings, Diagnostics and About. Until the launcher is implemented, existing navigation routes may remain available internally, but the final shell must not crowd every destination into the dock.

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

Playback transport belongs to Live. Show Details may expose Run only after a successful load if that workflow remains desirable, but it must never become a second full Live page.

## 5. Nodes

Purpose: network and device health.

Contains:

- SUE
- IAN nodes
- CHILDREN
- GRANDCHILDREN
- SAVIOUR controllers
- Device name and role
- Online/offline state
- Last seen
- Signal strength where available
- Firmware version
- Battery state where applicable
- Active warning/fault
- Rediscovery/rescan

Selecting a node opens progressively disclosed technical details such as MAC/device ID, IP address, uptime, sensors, outputs, last command and last response.

## 6. Audio

Purpose: Director-facing audio engine and zone control.

Contains:

- Local audio engine state
- Current track and playback position
- Master level
- Output connection state
- Level meters where available
- Play test, stop, mute and volume
- Remote audio node state
- Asset validation
- Safe audio-engine restart where supported

The Live page may display the current track, but detailed controls live here.

## 7. Logs

Purpose: operator and diagnostic history.

Contains:

- Timestamp
- Severity
- Source
- Event message
- Filters: All, System, Show, Audio, Network, Emergency
- Pause/resume live updates
- Clear and export actions
- Scrollable history

Desktop shows only a compact recent-events summary.

## 8. Settings

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

- Rescan SD
- Export/clear logs
- Firmware update
- Diagnostics
- Factory reset

Dangerous actions require confirmation and must not appear on the first settings view.

## 9. More launcher

Purpose: keep the permanent dock uncluttered.

Contains large destinations for:

- Nodes
- Audio
- Logs
- Settings
- Diagnostics
- About

The launcher contains navigation only, not duplicate status cards.

## 10. Emergency overlay

Purpose: unmistakable system-level safety state.

Contains:

- EMERGENCY STOPPED
- Show name
- State before emergency
- Current cue
- Elapsed and remaining time
- Stage connection
- Activation time
- Acknowledge
- Diagnostics
- Clear-state guidance

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
| Audio engine and zones | Audio |
| Configuration | Settings |
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
