# Showduino finish-line evidence — 18 November 2026

## Significant source change
GitHub main HEAD: `10c8dce353815f680783268e8293ebf87d7f1bf8` (18 November 20:29 UTC). Unlike recent daily generated-only updates, commit `504a7aa6793feefe5b34dd50e4ebdb41b9e111a2` (18 November 17:46 UTC) **merged Showduino v1.0 source reconciliation** (PR #72). A comparison from 14 November `f907670f` to current main shows 171 commits and extensive changes across P4, Audio, other node firmware, workflows, fixtures and documentation. This comparison is broad and does not certify each change. The two commits after the reconciliation merge only regenerate the S3 embedded Studio bundle and its build manifest.

## Documentation discrepancies / corrections
The 17 November User Manual opening says that main has only generated Comms updates and calls SHDO parsing (#63), pixel segment consistency (#64) and embedded Studio (#66) unmerged candidates. **That description is superseded by the 18 November reconciliation merge.** Do not carry forward prior PR-state claims without rechecking the merged code. The manual's 20 October source-reviewed revision is historical, not a current-firmware certification. This report updates the source status, not the full operating instructions; functional claims for changed modules require source-by-source review and hardware tests.

## Latest physical evidence (unchanged)
- First real combined audio/visual playback reported 29 October.
- Existing 22-second local show and local Emergency-clear correction owner-reported working.
- P4 and Audio Node pixels and audio demonstrated in earlier bench tests.
- No signed two-run P4-confirmed LOAD → START → COMPLETE acceptance.
- No signed Emergency interruption → safe outputs → physical release → Director-authorised clear → no automatic restart acceptance.
- Previous Director reset/navigation faults and MOSFET reconnect output drop remain retest items where relevant.

## Smallest non-DMX path
1. Preserve exact flashed local Director, P4, Comms and node binaries/configurations and the existing SD production/media. Do **not** flash newly merged main onto the bench merely because it is merged.
2. Compare the preserved flashed/local working sources with the merged main reconciliation; isolate only differences affecting the existing fixture and safety.
3. Run the existing 22-second fixture twice and record P4 LOAD, START, cue outputs and COMPLETE, with no unexpected Director reset.
4. With hazardous loads disconnected, repeat using physical Emergency during active playback; verify safe outputs, button release, authorised clear, and no auto-resume.
5. If a specific failure occurs, fix only that failure and repeat acceptance. No DMX expansion, optional FastLED upgrade or new node scope.

## Verdict
Source integration advanced materially on 18 November; physical acceptance has **not** advanced on the evidence available. GitHub main and flashed bench must not be assumed equivalent. No hardware was accessed or flashed by this documentation change.
