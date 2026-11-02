# Reconcile the flashed bench baseline and harden SHDO imports

This draft starts the complete integration roadmap by representing the owner's currently flashed local source on GitHub and applying two bounded SHDO fixes. It does not install OTA/FastLED, change partitions or flash devices.

## Preserved baseline

The owner confirms `showduino-reconciled/firmware/` is the source flashed across the active system. They run the existing 22-second full-node show, report Emergency clearance fixed and all nodes complete except the deferred C3 LED-only node.

Baseline source HEAD was `f1ce0c3a3169d8e3b279d293e883b2f1f5ffc481` plus tracked edits and untracked source. The complete working folder was archived with Git metadata, extracted independently, and every restored file verified by SHA-256. Its 904 working files remained unchanged during the audit and preparation. Board binary/settings/NVS identities and actual deployed SD asset hashes are not inferred from source confirmation.

The baseline representation includes existing local Audio MP3/volume/buffering and GPIO5/GPIO22 separation; Director Cue Library/checkpoint, clear recovery, runtime freshness, asset picker and theme-lifetime work; P4 ambience/scene/IO extensions; and existing Studio changes. These are preservation of the owner's local work, not newly commissioned features in this PR. Existing current-main manuals are retained with a dated candidate clarification.

This necessarily makes the total diff against main broader than the new parser patch. The preservation commit and follow-up correctness/test commit should be reviewed separately. Earlier stacked PRs #47–#50 describe portions of this baseline but are not a complete replacement for it.

## New functional changes

1. `protocol/showduino_shdo.h` now consumes exactly one JSON `action.value`. Previously, reading a value into an empty file buffer could then trigger a second skip because the buffer had become populated. Real website exports with `action.value` before `action.params` could fail to compile.
2. Generated commands and node prefixes now check formatting return lengths. A command that would overflow the 64-byte buffer is refused with `SHDO_COMMAND_TOO_LONG`, rather than accepted after silent shortening. A refused production exposes zero compiled cues through the public result count.

The implementation adapts `fix/w005-shdo-action-value-parser` at `4a15557acb69d18acb020e3d313dd44473fa4f04` and PR #36 at `e4df5e078cdffa48a8980bd3793fd265cb396212`; it does not replace the older branch's entire header. Local ambience routes, stable equal-time ordering, final time sorting, JSON escaping/value handling and restricted Emergency show-pixel cues remain.

## Regression coverage

- Real website-export fixtures, 16-segment slot limit, long filename refusal, complete 63-character commands and one-character-over rejection.
- Public compiler tests for action member order, explicit parameter precedence and malformed JSON/value refusal.
- Existing 22-second production compilation and persistent timeline roundtrip remain covered; its source file is unchanged and C3 LED-only physical acceptance stays deferred.
- Matching clearance, saved system-sound volume and runtime-freshness tests/docs recovered from pinned #48/#49/#50 heads. Production firmware for those features is unchanged.
- Older JavaScript test harnesses updated to understand the already-present ambience guard and urgent safety-send path; the working transport/production parser policy was not rewritten to satisfy the harness.
- Linux CI now runs the Director policy suite plus the local scene-recovery/ambience compiler extensions, rather than relying only on the Windows runner.

## Validation and limits

The host production and Director runners pass, as do 12 additional host-suite groups, 14 Director/Audio/Studio Node checks and the actual ambience command/playback suite with mocked I/O. The real website-export regressions were observed failing before the SHDO fixes and passing afterwards. Host tests do not prove RF delivery, flash programming, timing or actual sound/pixel output.

Full target-build status is recorded in the PR description after the build attempts. No successful target compile is implied by the passing host checks or historical logs. Board settings and combined normal/Emergency acceptance remain pending. This is a draft review candidate, not a release or a recommendation to flash.

The active Arduino build workflow compiles this branch's P4 and Director sketches on Linux. It uses core 3.3.12 and pinned Director reference libraries (LVGL 9.6.0, GFX Library for Arduino 1.6.9, TAMC_GT911 1.0.2 and Adafruit NeoPixel 1.15.5). These match the inspected installed library versions, not independently identified flashed-build settings. Build artifacts are review evidence, not automatically published releases or instructions to install. The older ESP-IDF proof-target workflow remains separate.

The next roadmap stages still include adapting recovered signed fleet OTA, removing its mandatory uncommissioned pixel-role assumption without skipping required safety stations, integrating FastLED in stages and separately reconciling authenticated Emergency persistence. None of those changes is hidden in this first update.
