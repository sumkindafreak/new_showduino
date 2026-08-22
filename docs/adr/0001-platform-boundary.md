# ADR 0001 — Platform vs Stage Runtime

## Status

Accepted — Architecture Frozen

## Context

Early Showduino work blurred the Director touchscreen UI with Stage hardware control. That coupling would force a rewrite whenever the panel, transport, or execution board changed.

## Decision

Split the system into two products with a clear boundary:

- **Showduino Platform** (`os2/`): Shell, Apps, Services, Commands, Events, Theme, Session, Production/Asset models, Compatibility contracts.
- **Stage Runtime**: cue execution, audio, DMX, GPIO, effects, timing, safety execution.

The Director is a **client** of the Platform. The Platform talks to a **Stage Runtime API**. Communication (ESP-NOW, UART, Ethernet, loopback) is an adapter, not the OS.

Even when Platform and Runtime share a board, the Platform behaves as if the Runtime is remote (Constitution Law 6).

## Consequences

- Hardware and transport can be replaced without changing OS contracts.
- Apps never touch GPIO, DMX, or audio engines.
- Simulator / web / larger Director panels can reuse the same Platform.
- Stage firmware evolves independently behind the Runtime API.