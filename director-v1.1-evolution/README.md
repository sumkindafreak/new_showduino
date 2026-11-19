# Showduino Director V1.1 — Evolution Lab

**Status:** ISOLATED RESEARCH & DEVELOPMENT — not production firmware, not bench-approved  
**Hardware:** JC8048W550C (ESP32-S3, 800 × 480 RGB, GT911)  
**Branch:** feature/director-v1.1-evolution  
**Created:** 19 November 2026

## Purpose

Investigate and test reliability, responsiveness, card design, graphics and effect previews for the Showduino Director, without modifying the currently working Director firmware. GitHub source reconciliation PR #72 merged on 18 November 2026; physical flashed-bench equivalence and full show/safety acceptance remain separate verification gates.

## Mandatory engineering rule

> Every Director upgrade must earn its place. If it looks impressive but compromises reliability, responsiveness or show safety, it doesn't make the cut.

Non-negotiable gates:
1. Reliability — no regressions in boot, display, storage, touchscreen or communications.
2. Responsiveness — no unacceptable changes to touch and display performance against the measured working baseline.
3. Show safety — P4 remains authoritative; emergency behaviour is never delayed, bypassed or altered by cosmetics.
4. Compatibility — preserve all proven working hardware and firmware behaviour.
5. Rollback — every proposed change must be independently testable and reversible.

**GitHub source reconciliation is complete (#72); do not merge any Director V1.1 runtime change into main without physical baseline evidence, relevant acceptance tests and explicit review.**

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
- **DEVELOPMENT_01.md** — details of first implemented prototypes and test commands.
- **components/DirectorV11Performance.h** — allocation-free, bounded timing measurements.
- **components/DirectorV11CardModel.h** — read-only, safety-prioritised status classification.
- **components/DirectorV11CardLvgl.h** — isolated LVGL 9 card prototype.
- **tests/test_director_v11.cpp** — native model and performance tests.

The existing offline parser is entirely host-side and uses no non-standard Python dependencies. The C++ models are also portable; the LVGL 9 card prototype is isolated source only. No device firmware is changed by this workspace.

## Planned stages

| Gate | Stage | Exit condition |
| --- | --- | --- |
| 0 | GitHub source reconciliation | Merged as #72; flashed bench and hardware acceptance still require verification |
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

**Important:** These are merged GitHub source references, not proof the physical Director is flashed with the same revision. Capture installed build provenance before hardware integration.

## Protection of the current firmware

This R&D branch adds files under director-v1.1-evolution/ only. Do not edit, move, reformat or mass-rewrite existing Director, P4, Comms or shared protocol source as part of creating the workspace. Treat every future integration as a separate reviewable change, with before/after evidence and a rollback path.

## Initial status

- [x] Dedicated GitHub branch and isolated workspace created.
- [x] Source-based scouting and phase plan written.
- [x] Offline first-pass serial-log analyser added.
- [x] GitHub source reconciliation merged (PR #72, 18 November 2026).
- [x] Prototype timing collector and safety-prioritised card model created; native C++ tests passed on host.
- [x] LVGL 9 card prototype created in isolation (not device-compiled).
- [ ] Physical flashed-bench firmware provenance and baseline confirmed.
- [ ] Baseline serial logs collected from physical Director.
- [ ] Actual touch/render/heap benchmarks collected.
- [ ] Any changes compiled/flashed/validated on Director hardware.
- [ ] Director V1.1 integrated with active firmware.
