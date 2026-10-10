# PR 27 / PR 28 integration

Base: current main `24193d6`, 10 October 2026. Incorporated both PR heads:
- PR 27: `abbffc2559c9396cd9df96cf39c2ce89b90d57ce`.
- PR 28: `577435ad44ac8d969fbf5d964f36fdc4a52c3c99`.

## Conflict decisions

Retain PR 27 Audio GPIO5 status / GPIO22 show routing, saved-count validation,
Emergency discovery/recovery, headphone switching, Outputs/readback, P4 I/O and
bus commissioning, Director delivery/run gates and ESTOP pixel persistence.
Retain both MIC controls and PR 28 AMBIENCE controls, including theme cleanup and
mutually exclusive popups. Preserve the shared ten-pixel fallback rather than
PR 28's redundant local definitions. Fixed Lamp Jewel remains seven pixels.

Regenerate the Comms assets from combined commissioning source and main's pinned
canonical Studio commit `8a53b1665eec19dc2230b68e98d315b9e3b45b33`.
DMX remains parked; no protocol, runtime or authoring DMX expansion.

## Integration repairs

The new Director parser assumed a path-bearing status packet. Current P4 emits
`AMBIENCE:STATUS:<state>:VOL=<n>:LOCK=<n>:ERROR=<text>`. Read VOL from that
packet and retain the fixed acceptance path; avoid a negative path-length offset.
A host regression compiles and executes the actual parser against status, volume,
Emergency and error packets. Both Director ambience pages continue receiving feedback.

Shorten the acceptance show's description to the existing compiler bound. The
actual package now compiles and reloads, retaining every physical cue command and
its time. Internal LOG cues use the existing runtime normalization; the final
completion cue remains at 22 seconds. Update older ESTOP-persistence wording and
the bench checklist to reflect P4 emergency WAV playback and no automatic resume.

## Validation

- Eleven original Director/node/Studio JavaScript suites passed.
- New actual Director ambience parser regression passed.
- All sixteen existing PowerShell suite groups covered: fourteen other groups,
  production suite, and Studio tests (executed directly with Node).
- Actual P4 ambience player and browser command-policy host tests passed.
- Actual all-nodes acceptance package compile/persistent roundtrip passed.
- All 42 embedded assets decompress to their declared lengths; JavaScript parses.
- Full board firmware build results are recorded below after completion.

Physical flashing, DAC output, radio delivery and combined bench acceptance remain
outstanding. This integration is a draft; it does not merge main or publish releases.

For the commercial-manual version clarification and operator-facing finish-line status, see [10 October manual addendum](manual/SHOWDUINO_FINISH_LINE_STATUS_2026-10-10.md).

## Status update — 10 October 2026 (after PR #29 creation)

**Source of truth:** `main` at `57c87ad861e1b715a6e81c8f6388a27926ac83ec`; draft integration PR #29 at `82cab2d9f635c033bd0fb0cdb180dc006fdf1dc0` before this documentation update. PR #29 was originally integrated against `24193d6`. The newer main changes regenerate the embedded S3 Studio WebUI; GitHub currently reports the draft PR as **conflicting**, so its generated assets must be reconciled against latest main before merge. Do not overwrite the integration's commissioning changes with an older generated bundle.

**Verified automated checks:** GitHub Actions *Active firmware and console host checks* (run 38088028598) and *Regenerate S3 Studio WebUI* (run 38088028537) both completed successfully on the PR #29 integration commit. These confirm host/asset checks only. The integration report records passing ambience player, parser, production persistence/SHDO roundtrip, and JS regressions. **No final Director/P4/Comms full target-build result or flashing result is attached to this review**; do not infer a board-build pass from host CI.

**Physical evidence:** The owner previously reported a wireless Emergency Node successfully triggering audible Audio Node emergency playback at approximately 7 m. This is a valuable individual hardware observation, **not** a completed acceptance run. No dated physical `all-nodes-bench-test.shdo` LOAD → RUN → COMPLETE pass, or mid-show Emergency re-run, is recorded here.

**Required non-DMX acceptance only:**

1. Resolve PR #29 against latest `main`, preserving both MIC and AMBIENCE controls, Director timeline delivery gates, Audio GPIO5 status/GPIO22 show separation, P4 ambience Emergency override, pixel-count settings, MOSFET control, and the current embedded Studio bundle.
2. Record full target builds for Director, P4 and Comms; flash the matched firmware set and confirm required node versions/assets. Saved commissioned pixel counts override defaults. The Lamp Jewel remains seven pixels.
3. Run [the fixed 22-second all-node production](../examples/productions/ALL_NODES_BENCH_TEST.md) to COMPLETE. Check every actual output and the Director's state, including audio continuity and any Nodes-page freeze.
4. Repeat the same show, asserting Emergency during an active cue: verify Audio Node stop, P4 emergency WAV override, Lamp local emergency audio, white pixel overrides (especially Emergency Node GPIO2), MOSFET safe OFF, and no automatic resume after Director clear.

**Still unverified on hardware:** Emergency Node pixels, stutter-free Audio Node playback, Director ambience controls, Lamp Emergency WAV, MOSFET OUT1, Nodes-page responsiveness and the complete production. Keep the integration in draft until these checks are recorded. No DMX or unrelated feature expansion is required.
