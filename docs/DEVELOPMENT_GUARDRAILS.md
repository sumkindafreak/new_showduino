# Showduino Development Guardrails

These guardrails help avoid common architectural and safety mistakes.

## Core guardrails

1. Do not move authoritative runtime state into WebUI, Director, or transport code.
2. Do not allow network loss to stop a running show.
3. Do not maintain multiple undocumented protocol parsers/formats for the same message type.
4. Do not edit generated WebUI artifacts when source files are the canonical edit point.
5. Do not replace a working subsystem during unrelated feature work.
6. Do not change hardware pins because another GPIO appears convenient.
7. Do not combine transport concerns with show-decision authority.
8. Do not hide degraded/offline/error state; surface it clearly.
9. Do not confuse simulated/test state with physical output state.
10. Do not claim hardware validation when only compile/tests were run.

## Safety-specific guardrails

- Safety/emergency changes are high-risk by default.
- Preserve physical input semantics and latch behavior unless explicitly requested and validated.
- Emergency override must remain stronger than normal UI/API/runtime convenience paths.
- Emergency clear paths must not bypass required conditions.

## Version/protocol guardrails

- Keep protocol definitions centralized in `protocol/`.
- Update sender and receiver implementations together.
- Review compatibility impact before merging protocol changes.
- Do not reuse accepted firmware version numbers for different binaries.

## Studio/WebUI guardrails

- Keep mobile usability as a first-class requirement.
- Preserve active-form state across polling/re-render cycles.
- Prevent credential leakage in logs, status JSON, or persistent browser storage unless explicitly designed.
- Keep UI visual state clearly distinct from authoritative runtime state.

## Optional Cursor hooks (documented, not installed)

No repository-level Cursor hook configuration was present during this task. To avoid incompatible or intrusive setup, hooks are documented here rather than installed.

Proposed lightweight hooks:
- Warn on destructive git commands (`reset --hard`, `clean -fd`, force checkout) during agent runs.
- Capture starting branch/SHA/status snapshot before substantial edits.
- Warn when protocol files under `protocol/` change and require sender/receiver checklist.
- Warn when safety/emergency files change and require high-risk validation checklist.
- Optionally run lightweight validation helper after Cursor pack file edits.

Non-goals for hooks:
- no auto-commit,
- no auto-flash,
- no forced full rebuild on every edit,
- no automatic repository mutation.
