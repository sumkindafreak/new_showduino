# Director V1.1 acceptance and no-regression gates

**Gate rule:** A compile is not a pass. New visual functionality stays isolated until the complete set of applicable tests passes on the reconciled physical hardware.

## Mandatory invariants

- [ ] P4 remains the sole authority for cues, timing, node authorisation and emergency state.
- [ ] Director request success is not shown as physical output success until P4/Node state confirms it.
- [ ] Emergency is visible immediately, suppresses decorative animation, and retains the required all-pixels-bright-white safety override in the relevant nodes.
- [ ] Physical emergency actuation cannot clear the latch. Clear requires correct release and Director confirmation.
- [ ] No automatic show resume after emergency.
- [ ] Show remains operational when Director is disconnected or powered off.
- [ ] The Director never hosts the authoritative Studio or assumes it is the S3 Comms Engine.

## Existing-board functional gates

- [ ] Cold boot, warm reboot and power-cycle reliably display the correct page.
- [ ] READY → main UI stays immediate; no screen fade at the documented freeze boundary.
- [ ] Startup order retains Wi-Fi/ESP-NOW before graphical heap pressure and SD before RGB DMA where required by the working baseline.
- [ ] Capacitive GT911 touch, calibration, wake behaviour, scrolling and emergency confirm remain correct.
- [ ] Missing/invalid SD card produces safe recovery without a boot stall.
- [ ] ESP-NOW loss, reconnect, stale state and confirmed P4 state behave correctly.
- [ ] No GPIO or display-driver configuration regressions.

## Performance gates

- [ ] Record render frame distribution, P95 touch response, max stall, internal heap, largest free internal allocation, PSRAM and reconnect timing on both baseline and candidate firmware.
- [ ] Define acceptable numerical budgets from the hardware baseline **before** accepting a feature.
- [ ] Preview renderer has explicit frame-rate, render-time and memory budgets; cannot dispatch cues.
- [ ] Previews automatically stop during emergency, unsafe memory pressure or high UI/transport load.
- [ ] A 24-hour uninterrupted hardware run shows no crashes, progressive memory loss or missed safety-state transitions.

## Safe delivery / rollback

- [ ] Each enhancement is independently reversible, with a clear feature flag where applicable.
- [ ] Record toolchain versions, sketch SHA, libraries, partition scheme and actual device build.
- [ ] Create and verify a recovery image / documented previous-firmware flash procedure.
- [ ] Review code diff for changes outside the isolated Director scope; changes outside must be separately justified.
- [ ] Do not merge into main until tested; approval remains explicit.
- [ ] Keep audit/bench migration separate from cosmetic upgrade commits.

## Evidence record for every proposed change

| Field | Required evidence |
| --- | --- |
| Change | What is proposed, source/link, why it improves operator workflow |
| Affected paths | Enumerate all modified firmware and protocol files |
| Baseline | Bench build SHA, hardware revision, libraries and reference measurements |
| Candidate | Test build SHA, flags and difference from baseline |
| Functional test | Checklist results and logs |
| Emergency test | Assertion, latch, clear, white override and no-resume results |
| Performance | Before/after numbers, sample conditions, outliers |
| Endurance | Continuous run duration, errors, memory trend |
| Rollback | Recovery build and test result |
| Decision | ACCEPT / REWORK / REJECT, with reason |

This document is a **gate checklist**, not evidence that tests have already passed.
