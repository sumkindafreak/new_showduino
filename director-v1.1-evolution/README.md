# Showduino Director V1.1 — Evolution Lab

**Status:** ISOLATED RESEARCH & DEVELOPMENT — not production firmware, not bench-approved  
**Hardware:** JC8048W550C (ESP32-S3, 800 × 480 RGB, GT911)  
**Branch:** feature/director-v1.1-evolution  
**Created:** 19 November 2026

## Purpose

Investigate and test reliability, responsiveness, card design, graphics and effect previews for the Showduino Director, without changing the currently working Director firmware during the GitHub ↔ bench crossover audit.

## Mandatory engineering rule

> Every Director upgrade must earn its place. If it looks impressive but compromises reliability, responsiveness or show safety, it doesn't make the cut.

Non-negotiable gates:
1. Reliability — no regressions in boot, display, storage, touchscreen or communications.
2. Responsiveness — no unacceptable changes to touch and display performance against the measured working baseline.
3. Show safety — P4 remains authoritative; emergency behaviour is never delayed, bypassed or altered by cosmetics.
4. Compatibility — preserve all proven working hardware and firmware behaviour.
5. Rollback — every proposed change must be independently testable and reversible.

**No runtime changes are permitted into main until the current GitHub/bench reconciliation has finished and the upgrade's relevant acceptance tests pass.**

## Authoritative boundaries

- Operator UI: JC8048W550C Director.
- Show authority and emergency policy: ESP32-P4 Show Engine.
- Network gateway: standalone ESP32-S3 Communications Engine.
- The Director sends requests; successful sending is not confirmation of P4 execution.
- A running show must continue without the Director.
- Do not migrate browser Studio onto the Director.
- Do not bring back UART fallback or reassign active GPIOs.
- Do not flash unrelated CYD board binaries onto this device.

## Scope of this directory

The entire new Director V1.1 effort lives here until integration is expressly approved:
- **SCOUTING_001.md** — research, links, compatibility and candidate prioritisation.
- **PHASE_01_BASELINE.md** — practical first milestone: objective measurements before upgrades.
- **ACCEPTANCE.md** — safety, UI, performance, long-run and rollback gates.
- **tools/bench_baseline.py** — independent offline parser for existing Director serial diagnostic logs.
- **tests/test_bench_baseline.py** — tests for the parser.

The tooling is intentionally host-side, has no non-standard Python dependencies and does not access firmware or modify a device.

## Planned stages

| Gate | Stage | Exit condition |
| --- | --- | --- |
| 0 | GitHub ↔ bench crossover | Confirm canonical working Director baseline |
| 1 | Performance & reliability evidence | Recorded reference metrics + failure tests |
| 2 | Reusable active-LVGL card system | One representative card passes functional/performance tests |
| 3 | Touch, scrolling and navigation refinement | Responsive operation, no boot-screen freeze |
| 4 | Small, optional effect preview | Render-only, bounded resources, auto-suspends for safety |
| 5 | Production polish | Usability and endurance tests pass |

Stage 2 initially targets the **active LVGL shell**. The repository contains an OS2 card component, but the active BoardConfig.h currently defaults SHOWDUINO_OS2_SHELL to 0. Do not confuse it with the shipping UI.

## Current, verified GitHub entrypoints

- Working Director: firmware/director-esp32-8048s050/ShowduinoDirector8048S050/
- Board map: .../BoardConfig.h
- Display buffer and flush: .../lvgl_port.cpp
- Capacitive touch: .../touch_lvgl.cpp
- Nodes page: .../page_04_nodes.cpp
- Diagnostics page: .../page_06_diagnostics.cpp
- OS2 reference component (not necessarily active): .../os2/widgets/Card.h

**Important:** These are GitHub references, not proof they match the latest hardware bench copy. Revalidate after the crossover.

## Protection of the current firmware

This R&D branch adds files under director-v1.1-evolution/ only. Do not edit, move, reformat or mass-rewrite existing Director, P4, Comms or shared protocol source as part of creating the workspace. Treat every future integration as a separate reviewable change, with before/after evidence and a rollback path.

## Initial status

- [x] Dedicated GitHub branch and isolated workspace created.
- [x] Source-based scouting and phase plan written.
- [x] Offline first-pass serial-log analyser added.
- [ ] GitHub ↔ bench baseline reconciled.
- [ ] Baseline serial logs collected from physical Director.
- [ ] Actual touch/render/heap benchmarks collected.
- [ ] Any changes compiled/flashed/validated on Director hardware.
- [ ] Director V1.1 integrated with active firmware.
