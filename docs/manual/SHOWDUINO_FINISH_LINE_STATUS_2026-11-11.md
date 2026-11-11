# Showduino finish-line evidence — 11 November 2026

**Source baseline:** [main at 8dafd877](https://github.com/sumkindafreak/new_showduino/commit/8dafd877185708c87990a546348082680070d416), 11 November 18:44 UTC. **Goal:** one complete repeatable non-DMX scenario; not a product release claim.

## Changes since 10 November
Four commits since efb2e13: two generated Comms S3 WebUI/manifest updates, a simplified C3 Lamp Node integration-spec commit and its merge via [PR #65](https://github.com/sumkindafreak/new_showduino/pull/65). The merge adds `docs/c3-lamp-node-simplified-integration.md`; it **does not deliver simplified C3 Lamp firmware or bench acceptance**. The compare also lists zero-line additions under `tools/production-tests/`; these are not evidence of passed tests. No active non-generated firmware implementation changed in this comparison.

## Newly important integration blockers outside main
- [PR #63](https://github.com/sumkindafreak/new_showduino/pull/63): the current main SHDO compiler reportedly rejects website Studio exports with `INVALID_JSON: invalid clip object`. Candidate repairs address `action.value` parsing, stable time ordering, command truncation and safety flags. Host tests pass; P4 bench test remains required. This is a **real finish-line blocker if the chosen scenario is exported from website Studio**, not if the existing persisted 22-second show plays correctly.
- [PR #64](https://github.com/sumkindafreak/new_showduino/pull/64): compiler and Studio differ in pixel segment allocation. Candidate aligns behavior; host tests pass, but recompiling some Studio-exported shows changes segment reuse. Existing SD-compiled productions are unaffected. **Do not flash/merge without pixel and Emergency bench tests.**
- [PR #58](https://github.com/sumkindafreak/new_showduino/pull/58): MOSFET reconnect recovery is still a candidate, not physically accepted. Only a finish-line blocker if the selected show requires that output.
- [PR #51](https://github.com/sumkindafreak/new_showduino/pull/51): preserved local firmware candidate remains separate from main; compare against actual flashed versions before integration.
- FastLED PRs and the simplified C3 Lamp are optional to this finish line.

## Latest physical evidence
- 29 October: first combined audio and visual playback achieved on hardware.
- 2 November: owner reported existing 22-second show running and Emergency clearance fixed in local flashed firmware.
- No later signed evidence of two consecutive P4-confirmed LOAD → START → COMPLETE runs, or mid-show Emergency assert/release/authorised clear with no automatic resume.
- Director resets/Cue Library navigation and MOSFET Director-reconnect output drop require targeted retests if they affect the acceptance path.

## Minimum acceptance path
1. Preserve exact working local flashed firmware and SD assets; avoid replacing the local Emergency fix with main or experimental branches.
2. Run the **same existing 22-second show twice**, logging P4 LOAD, START, all expected outputs and COMPLETE.
3. With hazardous loads disconnected, interrupt with Emergency; verify safe outputs, physical release, Director-confirmed clear and **no automatic restart**.
4. If using a new website Studio export, test PR #63 on isolated P4 and only then investigate #64 segment parity; otherwise keep the existing persisted show and defer compiler changes.
5. Retest Director stability and MOSFET reconnect only where the scenario depends on them. Integrate only bench-validated essential corrections.

**Classification:** combined playback achieved; local 22-second show and Emergency fix owner-reported; repeatable scenario and Emergency recovery acceptance outstanding. This document is source/bench evidence reconciliation, not proof of installed behavior.
