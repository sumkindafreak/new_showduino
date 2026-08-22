# ADR 0005 — App Registry

## Status

Accepted — Architecture Frozen

## Context

Hardcoding dock entries (Dashboard, Shows, Lighting, …) inside the Shell coupled chrome to domain knowledge and violated Law 5.

## Decision

Apps **register** with `AppRegistry`. The Shell builds the dock only from registered apps. Adding an app is registration, not Shell edits.

Search, launcher, permissions, and future plugins consume the same registry.

## Consequences

- Shell stays domain-ignorant.
- New applications are additive (Application Development phase).
- Closing an app changes no platform state (Law 3).