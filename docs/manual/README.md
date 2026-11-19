# Showduino V1 — Operating Documentation

**19 November 2026 source-based rewrite · Product `1.0.0-rc.1` · GitHub `main` `41c7c71` reviewed.**

This is the maintained **operator documentation entry point** for the currently reviewed Showduino V1 firmware. It replaces stacked historical status banners with coherent present-tense procedures and explicit references to pending physical acceptance.

> **Release gate:** The owner reports first combined audio/visual playback on 29 October, a local working 22-second show and a corrected local Emergency-clear path. Two witnessed P4 LOAD → START → COMPLETE runs and a complete mid-show Emergency → safe outputs → deliberate Director/P4 clear → **no auto-resume** test have **not** yet been signed off. Keep hazardous loads isolated, and do not claim public-operation readiness from these documents or a source merge.

## Read in this order

1. [Quick Start](SHOWDUINO_QUICK_START.md) — first connection, productions, SHDO deployment and Emergency.
2. [Full User Manual](SHOWDUINO_USER_MANUAL.md) — system behaviour, roles, safety, operation, troubleshooting, servicing.
3. [Director Operator Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) — verified source page names, Cue Library monitor and controls.
4. [Studio Cue Library](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md) — Website authoring versus embedded Studio, Director Cue Library and P4 compiler.
5. [Pre-Opening / Physical Acceptance Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) — reproducible non-DMX tests and release decision.
6. [Hardware and Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) — identify the actual installed PCB, wiring, code, media and spares.
7. [First Show and Evidence Guide](SHOWDUINO_FIRST_SHOW_AND_DOCUMENTATION_GUIDE.md) — protect the known-good flashed build; capture repeatable results.
8. [Claim-to-Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) — trace source implementation, documentation limits and physical status.
9. [Open Verification Gaps](SHOWDUINO_MANUAL_GAPS.md) — hardware tests, source/UI mismatches and incomplete workflows.
10. [Documentation Review Record](SHOWDUINO_DOCUMENTATION_REVIEW.md) — scope, source baseline and editorial acceptance.

## Source/architecture references

- [Constitution](../constitution.md), [Architecture](../architecture.md) and [Hardware Pinout](../hardware-pinout.md).
- [P4 Production Storage](../production-storage.md), [P4 SD](../p4-sd-storage.md) and [Studio Production Format](../studio/production-format.md).
- [Comms WebUI](../../web/showduino-studio/README.md), [Node Roadmap](../node-roadmap.md), [Physical Test Checklist](../physical-test-checklist.md).
- [Firmware roots](../../firmware/) and [shared protocol](../../protocol/).

## Evidence rule

The **P4 decides**, Comms transports, Director commands/displays and Nodes act. The owner-reported local flash, current GitHub `main`, unmerged candidates, embedded WebUI and current Website Studio are **not interchangeable baselines**.

Merged [source reconciliation PR #72](https://github.com/sumkindafreak/new_showduino/pull/72) changed the source situation after the old 20 October manual review. Documentation-only [PR #73](https://github.com/sumkindafreak/new_showduino/pull/73) was still open when this rewrite started. Source-review success, host CI and written guides cannot replace a signed physical Emergency/safe-output test.

Historical audits and earlier RC notes remain in Git history and dated evidence files, but **are not current startup instructions**.

**No firmware, load wiring or safety acceptance is changed by this documentation rewrite.**
