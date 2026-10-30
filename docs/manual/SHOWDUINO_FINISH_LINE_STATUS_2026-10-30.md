# Showduino finish-line evidence reconciliation — 30 October 2026

## Sources and scope

Repository: `sumkindafreak/new_showduino`; `main` observed at `a85f3cf8050118dbc69e363bfa70a212cfd8e48c` on 30 October. The recent visible commits regenerate the S3 embedded Studio WebUI bundle; they do not by themselves prove a physical firmware change. This is a documentation-only reconciliation, not a new firmware test or a complete code-to-manual certification.

## Verified evidence levels

- **Owner-reported physical milestone (29 October):** Showduino played its first show combining audio and visual effects. Some teething issues remain. Do not describe the system as never having played a show.
- **Previously owner-reported partial bench successes:** WAV audio and volume behaving correctly on the installed Audio Node; working P4 and Audio Node pixel effects. Exact installed firmware commits have not been recorded in this document.
- **Not formally accepted:** existing 22-second all-node production through LOAD → START → COMPLETE; complete timing and node-output record; Emergency asserted during audio, all outputs safe, Director-authorised clear, and no automatic restart.
- **Unresolved or not retested:** Director Core 1 LoadProhibited resets observed 28 October; microphone CALIBRATE and threshold acknowledgement; installed firmware version matching; remaining SHDO deployment and node compatibility findings. Do not infer that any unmerged fix is installed.

## Smallest non-DMX finish-line actions

1. Preserve and identify the exact firmware, SD assets and show used for successful playback.
2. Repeat the same production, recording LOAD, START, expected cues, COMPLETE, audio continuity and node output observations.
3. Reproduce and fix Director resets only if they remain present on this configuration.
4. Repeat with Emergency asserted mid-playback; record independent P4 latch, safe outputs, deliberate authorised clear and no automatic restart.
5. Sign the acceptance record and only then update release readiness claims.

## Documentation rules

The 20 October manual source baseline is retained as historical evidence, not silently relabelled as current. Keep implemented-in-main, proposed-on-PR, flashed-on-bench, observed-on-hardware and formally-accepted distinct. The Director selects and requests shows; P4 owns runtime and Emergency decisions; Comms transports; specialist nodes act and report. Emergency cannot depend on Director connectivity. Do not add DMX to this finish-line scope.

## Remaining documentation work

This reconciliation updates the operator entrypoints and records the first-show milestone. It does not claim to have checked every firmware symbol, pin mapping, document, release note, archived design or example in the repository. A full source-by-source documentation audit is still required before calling all repository documentation current.
