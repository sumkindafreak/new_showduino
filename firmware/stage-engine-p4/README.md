# Showduino Show Engine — Stage Controller (ESP32-P4)

```text
Status: ACTIVE
Role: Showduino Show Engine
Product: Stage Controller (ESP32-P4)
```

Canonical active Show Engine firmware. Folder name `stage-engine-p4` is temporary; rename planned.

```text
Director --ESP-NOW--> Communications Engine --UART--> this Show Engine
Node     --ESP-NOW--> Communications Engine --UART--> this Show Engine
```

## Constitution

> The Show Engine decides.

Owns (target): authoritative show state, timeline/cues, project storage, configuration, safety policy, Web UI / Web API / WebSocket, and node command lifecycle (accept vs complete).

## Maturity — do not overstate

Current sketch is an **early command hub**:

- Parses colon-text requests over UART
- Tracks simple flags (e.g. show running, emergency lock)
- Routes relay (and stub) work through the Communications Engine
- Returns basic ACK / status lines

It does **not** yet implement the full timeline engine, primary project store, DMX, real pixel/audio engines, or Web UI/API.

## Sketch

```text
firmware/stage-engine-p4/ShowduinoStageEngineP4/
```

## Policy reminders for later firmware work

- Absolute relay states only (no distributed `TOGGLE`)
- No false success for placeholder pixel/audio routes
- Address nodes by logical device ID at the application layer
- Publish state changes; treat Director/browser input as requests
- Running show must not require Director or browser presence

See [`docs/constitution.md`](../../docs/constitution.md), [`docs/architecture.md`](../../docs/architecture.md), and [`docs/repository-status.md`](../../docs/repository-status.md).
