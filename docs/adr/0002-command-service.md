# ADR 0002 — CommandService

## Status

Accepted — Architecture Frozen

## Context

Apps initially called domain methods such as `ShowService.load()`, mixing **intent** with **truth**. That blocked validation, history, macros, remote execution, and clear logging.

## Decision

Introduce **CommandService** as the sole intent path:

- Services answer questions ("What is the current production?").
- Commands express intent (`LoadProduction`, `StartShow`, `EmergencyStop`, …).
- Events announce what already happened (`ShowStarted`, not `StartShow`).

Commands are validated, queued, drained by Communication, and recorded in history. Domain services remain read/truth only.

## Consequences

- Central place for permissions, auditing, automation, and undo (where safe).
- Apps stay disposable (Law 3); they never own pending Stage operations.
- New operator actions add **commands**, not new coupling into ShowService.