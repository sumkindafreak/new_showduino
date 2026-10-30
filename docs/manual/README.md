# Showduino operating documentation

> **30 October 2026 status:** On 29 October the owner reported Showduino's **first real combined audio-and-visual show playback** on physical hardware. This supersedes statements that no integrated show has ever played. It is **not** a signed pass of the 22-second all-node LOAD → START → COMPLETE fixture or mid-show Emergency repeat. Owner-reported working hardware includes Audio Node WAV playback and P4/Audio Node pixel output. Exact flashed firmware versions and complete timing logs remain unrecorded here. Previous Director Core 1 LoadProhibited resets and Audio microphone CALIBRATE/threshold acknowledgements remain unresolved until retested. Distinguish current main, unmerged PRs and flashed firmware. The 20 October source baseline remains historical. See [30 October reconciliation](SHOWDUINO_FINISH_LINE_STATUS_2026-10-30.md).

Revision 0.3-RC · reviewed 20 October 2026. Start here and match the documented baseline to the firmware installed on your unit.

- [Quick Start](SHOWDUINO_QUICK_START.md) — power, network, Studio Cue Library, create/export/persist/load/start, Emergency.
- [Studio Cue Library Guide](SHOWDUINO_STUDIO_CUE_LIBRARY_GUIDE.md) — searchable timeline presets, drag/drop, Inspector routing, compatibility and Director distinction.- [Quick Start](SHOWDUINO_QUICK_START.md) — power, network, create/export/persist/load/start, Emergency.
- [User Manual](SHOWDUINO_USER_MANUAL.md) — complete RC operating reference and boundaries.
- [Director Operator Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md) — everyday touchscreen operation.
- [Pre-Opening Checklist](SHOWDUINO_PRE_OPENING_CHECKLIST.md) — repeat before guest operation.
- [Build and Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) — identify actual installed boards and record evidence.
- [First Show and Documentation Guide](SHOWDUINO_FIRST_SHOW_AND_DOCUMENTATION_GUIDE.md) — capture the build and test one simple show.
- [Verification Gaps](SHOWDUINO_MANUAL_GAPS.md) — unfinished implementation and physical acceptance.
- [Source Matrix](SHOWDUINO_MANUAL_SOURCE_MATRIX.md) — claim lineage.
- [October documentation review](SHOWDUINO_DOCUMENTATION_REVIEW.md) — exact baseline, corrections, checks and remaining limits.

Old Word/PDF/downloaded copies do not update themselves. Use this maintained set; source review is not physical acceptance or a final product release.

## Latest finish-line status

- [29 October 2026 — first real audio/visual show played](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md) — owner confirms physical combined audio/lighting playback; teething issues remain. `main` at `e57870c` adds only four Comms bundle regenerations since 28 October. Formal complete-scenario repeat and Emergency acceptance are still open; do not erase the achieved first-show milestone or mistake it for full release sign-off.
- [28 October 2026 addendum: physical audio/pixel progress and Director reset blocker](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md) — `main` at `fd0834ef` (four more generated Comms bundle commits, unchanged content hash). Owner now reports clean Audio Node WAV/volume and working visual pixels, but the newly flashed Director repeatedly crashes and microphone calibration lacks confirmation. Full 22-second all-node and Emergency acceptance remain open. The 20 October manual source baseline remains dated.

