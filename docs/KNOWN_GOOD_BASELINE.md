# Showduino Known-Good Baseline

This document separates source state from hardware state.

## Current recorded baseline (from repository evidence only)

- **Latest source:** available in repository history (current branch/HEAD at time of inspection).
- **Latest compiled:** UNKNOWN / NOT RECORDED in this file by default.
- **Latest flashed:** UNKNOWN / NOT RECORDED.
- **Latest bench verified:** UNKNOWN / NOT RECORDED.

If the repository cannot prove a firmware image was physically flashed or bench-tested, do not infer it from source edits.

## Why this separation matters

- Source edits prove only that code changed.
- Compile success proves buildability only.
- Flashing proves firmware write only.
- Bench verification proves observed hardware behavior.

## Update template for future bench sessions

Use this template after real validation sessions:

```text
DATE/TIME:
OPERATOR:
LOCATION/BENCH:

LATEST SOURCE
- Branch:
- SHA:

LATEST COMPILED
- Components built:
- Commands used:
- Result:

LATEST FLASHED
- Component(s):
- Firmware version(s):
- Hardware target(s):
- Flash method:
- Result:

LATEST BENCH VERIFIED
- Scenario(s) tested:
- Pass/fail:
- Evidence/log references:

LATEST RELEASE VERIFIED
- Regression/acceptance suite run:
- Pass/fail:
- Notes:
```
