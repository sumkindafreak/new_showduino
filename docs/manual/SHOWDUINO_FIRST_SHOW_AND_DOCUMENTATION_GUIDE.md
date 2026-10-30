# Showduino: Weekend Build and First-Show Guide

**Documentation review:** 20 October 2026 · firmware source `f4c9aeaabdcf939417f9891fb01d98e1e5d8aec2` · [review scope and evidence](SHOWDUINO_DOCUMENTATION_REVIEW.md). Source review does not establish physical acceptance.

Product: 1.0.0-rc.1 / SHDO v2. Status: documentation work in progress.

This guide accompanies the [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md), [User Manual](SHOWDUINO_USER_MANUAL.md), [Quick Start](SHOWDUINO_QUICK_START.md), [Director Guide](SHOWDUINO_DIRECTOR_OPERATOR_GUIDE.md), and [Manual Gaps](SHOWDUINO_MANUAL_GAPS.md).

## Capture your installed build

1. Photograph the three main boards in the central enclosure: Director S3 touchscreen, dedicated Comms S3, and P4 Show Engine.
2. Photograph Audio, Lamp, Pixel, Emergency and MOSFET nodes individually. Capture both PCB sides, connectors, and fitted enclosures.
3. For every part, record exact PCB revision, manufacturer, supplier, quantity, voltage, role, mounting, and spare availability.
4. Record firmware Git commit, board manager core version, installed libraries, FQBN options, and physical test evidence per node.

Use the [Parts Register](SHOWDUINO_BUILD_AND_PARTS_REGISTER.md) as the working source for pinouts and vendor/firmware references. Do not claim a representative product photo is a verified photo of the installed module.

**24 October status:** Use the [finish-line audit](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md) for the distinction between `main` and draft PR #29's fixed 22-second all-node test. The physical normal run and mid-show Emergency repeat are **NOT YET VERIFIED**. Do not start a different production or add new features to close this gate.

**29 October physical milestone:** Showduino has now **played its first real combined audio/visual show on physical hardware**, as confirmed by the owner. That success must not be erased by the older 22-second fixture's `NOT RUN` status. Teething issues remain and the owner has not yet recorded a complete, repeatable P4-confirmed scenario or mid-show Emergency recovery. **For the smallest finish-line test, preserve today's working production and firmware, repeat that same show through COMPLETE, then test Emergency interruption and deliberate clear with no automatic restart.** Do not introduce new features or additional hardware for this gate. See the [29 October audit](SHOWDUINO_FINISH_LINE_AUDIT_2026-10-24.md).

## First show: safe workflow

1. Consult the [Quick Start](SHOWDUINO_QUICK_START.md) and power the P4, Comms, Director, then each specialist node.
2. Confirm the link and P4 authority before using the Director.
3. Create one simple scene and one timed Pixel or Audio clip inside Studio.
4. Validate and export SHDO v2. Refer to [show format](../show-file-format.md) and [Studio documentation](../studio/README.md).
5. Verify which authored cue types the installed P4 firmware can deploy and execute; Studio preview alone is not physical proof.
6. From Website Studio, export the `.shdo`, open the saved Communications S3 address, then use **Productions → Persist SHDO v2 to P4 SD → Persist to SD**. Wait for the P4 confirmation, separately **Load** the production, then explicitly **START** and **STOP**. Record actual hardware responses. See the [Quick Start workflow](SHOWDUINO_QUICK_START.md#website-studio--installed-production).
7. Test Emergency assertion, operator-authorised clear, blackout or override, and no automatic show restart.

Showduino Emergency is a theatrical show-control function and is not a certified life-safety system. Isolate risky loads during bench tests.

## Record physical acceptance

| Test | Status (fill in after bench test) |
| --- | --- |
| P4 boot, SD, system audio | NOT YET VERIFIED |
| Comms S3 UART and ESP-NOW | NOT YET VERIFIED |
| Director screen and touch | NOT YET VERIFIED |
| Audio Node WAV and Emergency | NOT YET VERIFIED |
| GPIO23 show pixels and GPIO24 signage | NOT YET VERIFIED |
| C3 Pixel and Emergency Nodes | NOT YET VERIFIED |
| Lamp Node | NOT YET VERIFIED |
| MOSFET physical DC outputs | NOT YET VERIFIED |
| Full show plus recovery | NOT YET VERIFIED |

For each PASS, save a dated log, photograph, voltage reading if applicable, node identity and Git commit. Keep personal purchase records and credentials out of public documentation.

## Finish-line checklist

The weekend documentation is complete when a technician can find the part, identify the correct PCB variant, locate its firmware, understand its wiring, restore the configuration, and repeat its commissioning test without asking the original builder.
