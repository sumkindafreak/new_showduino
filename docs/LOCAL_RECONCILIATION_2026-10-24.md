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

### Fresh P4 target build

ESP32 Arduino core 3.3.11, FQBN `esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3`: PASS. Flash 1,171,564 / 1,310,720 bytes (89%); static RAM 79,780 / 327,680 bytes (24%). Built with one compiler job. No hardware was flashed.

## Handoff

The clean user-facing Git checkout is `outputs/showduino-reconciled`, on branch `reconcile/local-github-20261024`, with GitHub origin configured and current main history incorporated. It is the updated review/bench source copy. Original Desktop and linked working folders remain intact. The separate OTA/UI/MOSFET edits were preserved, not silently folded into the acceptance candidate. Build logs and host-suite result inventory are saved under `outputs/validation`.

Do not treat this candidate as a fully accepted release. Fresh Director/Comms/C3 target builds and physical Emergency/Director-loss acceptance are not established by the passing Audio/P4 builds or host tests. No push, main merge or flashing occurred.

## 27 October 2026 — Audio Node 0.4.8 volume and Emergency spacing

User hardware logs show Emergency reception, HEADPHONE output, increasing WAV
read position/byte counters, no reported underruns and no playback error, but no
sound was heard. These logs do not establish a working physical headphone output.

Changed the Audio Node percentage-to-DAC mapping from a linear 96 dB attenuation
range to signal-amplitude percentage, quantised to half-dB steps. 50% now maps to
-6 dB instead of -48 dB; 100% retains unity gain with no boost. Zero percent
explicitly mutes the DAC, including when unmute is requested at zero. Saved
percentages, fades, ducking, output selection and ownership rules are retained.
Existing percentages are substantially louder: start the physical check at low
Director volume with headphones off your ears, then raise gradually.

Emergency playback still starts immediately, but now queues eight seconds of
silent stereo PCM after every complete announcement before repeating. Normal
show loops keep their immediate repeats. No blocking eight-second sleep was
added. Existing Emergency latch/duplicate-message protections remain, and clear
stops playback and cancels any remaining gap without automatically restarting
normal audio or the show. The change is on the Audio Node only.

Validation: Audio protocol host suite, amplitude curve/quantisation tests, actual
codec volume/mute/PA functions and actual playback service against mocked I/O
passed. Playback tests cover immediate first data, consecutive eight-second gaps
at 8/16/22.05/32/44.1/48 kHz for mono/stereo, partial writes, normal loops and stop
cancellation. Headphone auto-routing and WebUI commissioning regressions passed.

Full ESP32-A1S target build PASS using the same 4 MB / DIO / 40 MHz / no-PSRAM /
min_spiffs FQBN: flash 1,166,262 bytes (59% of 1,966,080), static RAM 60,688 bytes
(18% of 327,680). Log: `outputs/validation/Audio-0.4.8-volume-gap-build.log`.
Source is updated in `outputs/showduino-reconciled`; the board has not been
flashed automatically. Audible output, perceived volume, real repeat spacing
and clear-during-gap behaviour still require physical verification after flash.

## 27 October — MP3 branch alignment candidate

Integrated `origin/feature/audio-a1s-mp3` (`e8ea73f`) into the local reconciliation
branch, retaining the 0.4.8 volume mapping and eight-second Emergency WAV gap.
Audio candidate: `0.5.0-mp3-rc2`; matched P4 candidate: `0.6.13-mp3-rc1` for
Director-managed MP3 path validation. The original 22-second WAV fixture and
Studio assets are unchanged. No GitHub main merge or automatic flashing.

All 16 host suite groups passed; the new adapter checks use mocked decoder/I2S,
so physical MP3 decode, playback continuity and Emergency timing remain open.
The owner confirmed visible NeoPixel output on Audio Node and P4; these individual
visual passes are recorded in the bench result record.

Full target builds are pending. The first Audio attempt failed because a shared
Arduino temporary dependency folder disappeared. An isolated Audio/P4 build is
running; consult `outputs/validation/MP3-target-results.json` and target logs for
its eventual result. Do not treat this source merge as a completed firmware build
or hardware acceptance. Details: `docs/audio-mp3-local-alignment-2026-10-27.md`.
