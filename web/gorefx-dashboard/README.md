# GoreFX Dashboard for Showduino v1

```text
Status: LEGACY / SUPERSEDED / DEMO ONLY
Canonical browser UI: web/showduino-studio/ hosted by the Communications S3 SoftAP.
This dashboard records mock commands only — it is NOT connected to /api/command.
```

This folder retains the earlier GoreFX dashboard idea. Do not treat it as the current Showduino Studio. Buttons append to an in-memory mock log; they do not control a P4.

## Purpose

Historical / demo React shell for dashboard UX experiments.

It should **not** be presented as live Showduino control unless wired through the same SoftAP API documented in `docs/browser-control.md`.

## Source References

Primary source repo:

```text
sumkindafreak/gorefx-shadow-control
```

Secondary references:

```text
sumkindafreak/showduino-fx-control
sumkindafreak/showduino-scare-control-system
```

## Technology Direction

The strongest current dashboard base uses:

- Vite
- React
- TypeScript
- Tailwind CSS
- shadcn/ui

Canonical product path for live control is **SoftAP Studio** (`web/showduino-studio/`), not this mock.

## Initial v1 Scope

The first dashboard version should only do this:

1. Let user enter Showduino controller IP address.
2. Send test command.
3. Trigger Relay 1 ON.
4. Trigger Relay 1 OFF.
5. Trigger Relay 1 PULSE.
6. Trigger Emergency Stop.
7. Show command log.
8. Show connection status.

Only after this works should the full timeline editor and show library be added.

## Planned App Sections

```text
Dashboard
Live Controls
Timeline Editor
Show Library
Audio Manager
Diagnostics
Settings
About
```

## API Design

The dashboard should not know low-level hardware details directly.

It should send Showduino commands through a clean API layer.

Example frontend call:

```ts
sendCommand("RELAY:1:ON")
```

Possible controller endpoint:

```text
GET /command?cmd=RELAY:1:ON
POST /api/command
```

## Safety Requirement

Emergency stop must always be visible in the dashboard once hardware control is enabled.
