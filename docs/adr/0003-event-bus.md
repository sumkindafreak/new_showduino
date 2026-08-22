# ADR 0003 — Event Bus

## Status

Accepted — Architecture Frozen

## Context

Revision polling (`if (revision != last)`) scaled poorly and invited apps to invent their own change detection.

## Decision

An **Event Bus** sits between Services and Apps:

- Services publish after truth changes.
- Apps subscribe and update presentation.
- Events name the past tense (`ShowStarted`, `CueChanged`, `NodeLost`).

No polling unless genuine hardware requires it (Law 6 / Guarantee 6).

## Consequences

- Dashboard and Shell update only when relevant events fire.
- Many apps can listen without shared timers or duplicated caches.
- Event vocabulary is part of Compatibility contract v1 (`Api::EventBus`).