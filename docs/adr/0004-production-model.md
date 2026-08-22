# ADR 0004 — Production Model

## Status

Accepted — Architecture Frozen

## Context

Treating content as "show files" or SD folders made Library a file manager and tied the OS to one storage layout. Showduino aims beyond theatre into attractions, museums, and permanent installs.

## Decision

**Production** is the core object of the OS (not a path, not a JSON file):

- Stable **Production Manifest** schema (Compatibility v1).
- **AssetService** exposes a catalogue; backends (SD, USB, network, cloud) map into manifests.
- **Library.app** browses Productions; **Dashboard** monitors runtime via ShowService.
- Load is a **Command** (`LoadProduction`), never file I/O from an app.

## Consequences

- Vocabulary: Production / Library — not show file / browser.
- New venues reuse the same object model.
- Storage evolution does not change apps.