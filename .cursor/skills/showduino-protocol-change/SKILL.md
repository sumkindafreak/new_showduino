# showduino-protocol-change

## Purpose
Safely modify shared protocol behavior without creating incompatible or undocumented drift.

## Workflow
1. Run preflight and identify protocol source files in `protocol/`.
2. Enumerate all firmware/tools senders of changed messages.
3. Enumerate all receivers/parsers of changed messages.
4. Evaluate backward compatibility and migration impact.
5. Update shared protocol definitions first.
6. Update all parser and serializer/emit paths.
7. Update protocol version markers when required.
8. Update protocol/architecture documentation.
9. Compile every affected firmware target.
10. Run relevant host tests (at minimum protocol tests).
11. Report compatibility impact and any deferred migration work.

## Guardrails
- No duplicate protocol dialects.
- No sender-only or receiver-only protocol edits.
- No undocumented wire-format divergence.
