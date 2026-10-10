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
