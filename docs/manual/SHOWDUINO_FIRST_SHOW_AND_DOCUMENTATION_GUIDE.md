# Showduino V1 — First Show and Build Evidence Guide

**Rewrite 19 November 2026 · Source `41c7c71` · RC documentation, not physical sign-off.**

## Use the working show as your starting point

The owner reported the first combined physical audio/visual Showduino playback on **29 October 2026**, then a working **22-second local production** and a corrected local Emergency-clear path. That achievement remains valid as an observation. It is **not** a signed two-run/full-Emergency acceptance.

For the **next bench gate**, use that same production and untouched local flashed firmware, not an ambitious new show. The purpose is to prove **repeatability and safe recovery**.

## Step 1 — Preserve the known-good bench

1. Save the P4, Comms, Director and all active Node firmware binaries/configuration, with source hashes where known.
2. Copy the working P4 SD `/showduino/` tree, every Audio/Node media card and show assets; preserve originals.
3. Photograph each board's silk, fitted wiring, power supplies, connectors, antenna/ground arrangement and enclosures.
4. Record versions, Arduino board core/FQBN, libraries and physical node identities in the [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md).
5. Do **not** replace installed code with current GitHub `main` as part of this documentation work.

## Step 2 — Perform a controlled complete run **twice**

1. Isolate hazardous effects; use safe observation or low-power loads. **Arrange a witness to observe each entire run in person**; a signature added later without observing that run does not count.
2. Boot P4 → Comms → Director → Nodes; confirm fresh **P4 state** and all production-required nodes.
3. Select and **LOAD** the already-persisted 22-second show. Record P4 acknowledgement.
4. **START** it from an authorised operator surface. Capture P4 state, cue timing, Audio Node output, pixel behaviour and any safe MOSFET feedback.
5. Capture **P4 COMPLETE**, safe end state and timestamps. Have the witness record their name, observation, time and signature against **Run 1** before the next run.
6. Repeat from a fresh loaded/ready state **without firmware or show edits**, with the witness observing **Run 2** as well. Record the second P4 result and have the witness sign **Run 2** separately at the time.

A visual/audio performance without an authoritative start-to-complete log **or direct witness observation of each run** is useful progress but **not** this gate's PASS.

## Step 3 — Test the Emergency separately

1. While the controlled test show runs, press the **main physical P4 Emergency** button.
2. Confirm latched Emergency, correct safe outputs and stopped programme audio. Release the button and confirm latch remains.
3. When safe, request **CLEAR EMERGENCY** on the Director; deliberately **CONFIRM CLEAR**, then observe actual P4 acceptance.
4. Confirm **NO AUTOMATIC RESUME**. A deliberate new start/resume may be tested afterward as its own action.
5. Log failures; don't reboot as a substitute for clear or claim completion from a temporary workaround.

The main Emergency **8-second hold** triggers **Locate**, not a clear gesture. Separate venue safety systems must remain functional at all times.

## Step 4 — Only then exercise a newly authored show

Use Website Studio → export **SHDO v2** → Comms **Productions → Persist SHDO v2 to P4 SD → Persist to SD** → separate **Load** → **Start**.

Do not assume every Studio preset is accepted by the installed compiler. Follow the [Studio Cue Library Guide](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md) and record compile acceptance/rejection. The Director's Cue Library is a **live monitor**, not the Website Studio editor.

## Step 5 — Record the evidence, not just a PASS word

| What to capture | Required details |
| --- | --- |
| Firmware provenance | Source/flash versions, relevant Git SHA, hashes, board revisions |
| Original SD/media | Backup location, production ID, assets |
| Both full runs | Start/end time, P4 LOAD/START/COMPLETE, cue evidence, physical outputs |
| Emergency test | Press/release time, P4 latch, node safe state, Director request/confirm, no auto-resume |
| Director stability | Navigation, Cue Library close/return, absence of resets |
| MOSFET reconnect | Expected held state vs Actual and measured safe-load output |
| Acceptance decision | Dated witness, PASS/FAIL, remaining defects, repeat plan |

No document can self-certify safety. Use the [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) for the signed gate and [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) for code provenance.

## Definition of operator-ready documentation

Another competent technician should be able to identify each fitted module, find its correct firmware, understand the safety boundaries, connect/show the source-backed operator workflow, restore from backup and repeat the tests **without asking the original builder**. Incomplete physical results are marked **PENDING**, not described as passed.
