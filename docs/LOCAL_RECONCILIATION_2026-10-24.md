# Showduino local reconciliation — 24 October 2026

## Source and preservation

Original workspace: `C:/Users/tjpro/Desktop/showduino/new showduino`.
Original active branch: `feature/system-ota-completion-20261008`, HEAD `64d02f1`.
The original checkout remains on that branch with its eleven modified files and all untracked files intact. Fetch updated remote references only. No reset, stash, main merge, flash, release, OTA activation or push was performed.

`outputs/showduino-original-history.bundle` preserves all Git refs, including unpublished history. `outputs/local-working-changes.patch` preserves tracked edits. `original-local-status.txt` lists untracked files, which remain in the original folder (the bundle/patch do not back up untracked models, ZIPs or test directories).

The linked `repo-audio` copy has two uncommitted P4 changes (version label and restricted ESTOP pixel cue acceptance), preserved in `outputs/linked-p4-working-changes.patch`; PR #29 already supplies the restricted pixel-cue parser. Linked `repo-audio-crash` is clean. Neither linked checkout was changed.

Unpublished work includes signed OTA, recovery/rollback gates, Director delivery diagnostics and preserved Outputs controls. Local uncommitted work includes replay navigation, production scrolling, theme object lifetime changes, ambience parsing, a storage limit change and replacement MOSFET identifier driving. These remain separate from this candidate because their full behaviour has not been established against the newer integration. Nothing is silently discarded or replaced by GitHub.

## Candidate composition

Branch `reconcile/local-github-20261024` starts from draft PR #29 head `3d8a8877d6fca4fe5a9dcbb75716ab404dbd57b8` and merges GitHub main `89b89954dcde78769e1506a8ead076b7c7387eb6` locally. A subsequent automatic main regeneration, `3ae48a4b6a8e14d2410d1aa1bd85abaa3944e1d9`, changed only two timestamps and was incorporated while retaining the candidate assets/manifest. This is a review/bench candidate, not an approved production release.

Main's newer operator manuals are retained; integration-specific details are corrected by this record and the updated Audio commissioning paragraph. Main's exact Studio source revision `850dea084320a45fb89b584507eda1514662b8dd` is pinned while regenerating the embedded bundle with the integration's Outputs/ambience/I/O controls. All 42 embedded assets decompress to their declared lengths and embedded JavaScript parses. The DMX editor payload is byte-for-byte identical to main.

PR #40 head `cfea431f853ed7e95602fbc5ed652c75b229e9db` is compared and selectively adapted rather than merged wholesale: PR #29 already contains the separate Audio drivers. GPIO5 stays a dedicated one-pixel status indicator; GPIO22 stays the independent show line; KEY6 is disabled. The common policy normalises programmable line counts below ten to ten and retains larger valid counts. P4 show output enables at boot. Fixed indicators, the seven-pixel Lamp Jewel and 100-pixel P4 emergency signage retain their physical counts. The helper is shared across Audio, P4, C3 Pixel and C3 Emergency; boundary regressions cover missing, smaller, ten, larger and invalid counts. Explicit count replies and persisted values use the same normalised count.

## Architectural contract

One Director. One coordinated show. Multiple replaceable nodes.
The Director exclusively selects and operates shows. Nodes execute assigned commands; no remote node independently changes scenes. P4 and emergency protections must operate without Director availability. Emergency clear never automatically restarts a show or audio.

The existing independent protection paths are retained. Software checks do not prove radio delivery, electrical safety or physical response.

## Acceptance scope

Use the existing `examples/productions/all-nodes-bench-test.shdo`, ending at 22 seconds. No DMX expansion or new scenario. PR #29's short description and explicit 19.5-second MOSFET OFF are retained. Run LOAD → RUN → COMPLETE and record each real output, then repeat with Emergency during the active 10–14-second audio cue. Also assert Emergency with Director unavailable. Verify Audio stops, P4 emergency audio behaviour, full emergency pixel white, MOSFET safe OFF and no automatic restart after Director clear/reconnect. Record outcomes in `docs/manual/SHOWDUINO_BENCH_RESULT_RECORD.md`.

## Validation

All 16 existing PowerShell host suite groups passed. All discovered Director/Audio CJS regressions and both Studio JS regressions passed. The actual Director ambience parser, production persistence/SHDO roundtrip and P4 ambience/browser emergency policy passed. Added minimum-count boundary regression passed. Target-build results are recorded below after completion. Physical flashing and combined acceptance remain outstanding. Keep the candidate off main until those results are reviewed.

### Fresh Audio target build

ESP32 Arduino core 3.3.11, FQBN `esp32:esp32:esp32:PSRAM=disabled,FlashSize=4M,PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40`: PASS. Flash 1,163,338 / 1,966,080 bytes (59%); static RAM 60,360 / 327,680 bytes (18%). Built with one compiler job. No hardware was flashed.

Fresh Director compilation was cancelled during dependency processing to focus on the changed P4 target; Comms was not freshly compiled. Their earlier PR #29 build results remain historical evidence only. C3 targets were not freshly built in this session. The shared policy passes host boundary checks, but combined firmware/hardware acceptance is not claimed.
