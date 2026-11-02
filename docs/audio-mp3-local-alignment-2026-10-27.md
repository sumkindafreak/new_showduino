# Audio Node local MP3 alignment — 27 October 2026

## Source composition

The owner requested merging `feature/audio-a1s-mp3` into the reconciled local
copy. This merges remote branch head `e8ea73f` into local Audio Node 0.4.8,
retaining its history rather than replacing the sketch directory. The source
reports **0.5.0-mp3-rc2**. P4 reports **0.6.13-mp3-rc1** for its matching routing update. A recovery branch
`safety/pre-mp3-audio-048-20261027` retains the pre-merge source at `01b7154`.
GitHub main and the original Desktop checkout are not changed by this merge.

## Retained and aligned behaviour

- WAV and MP3 dispatch, file selection/inventories and MP3 capability reporting.
  Only WAV and MP3 are supported; no AAC/FLAC/OGG capability is implied.
- P4 and the Audio Node use a shared asset-path validator: relative node paths,
  absolute /showduino/audio/ paths and the existing Director show-library root
  work consistently for WAV/MP3. Traversal, sibling-prefix and arbitrary roots
  remain rejected. The older P4 firmware rejects MP3 extensions, so both matched
  firmware builds must be flashed for Director-driven MP3 playback. Director
  and Comms sources are unchanged.
- Installed decoder dependency: ESP8266Audio 2.4.1, used without PSRAM.
- Newer bounded/recursive local inventory paths and one-file wire replies are
  retained, avoiding a return to the older six-file/short-path branch behaviour.
- 0.4.8 amplitude volume curve and explicit zero-volume mute remain intact.
- Audio GPIO5 stays one status pixel; GPIO22 remains a separate show strip with
  the common ten-pixel minimum. Other node wiring is not changed.
- The Emergency asset remains `show_machine/emergency.wav`, starts immediately
  and repeats after eight seconds of queued silence. Repeated Emergency messages
  do not restart it. Clear cancels playback/repeats without resuming the show.
- The MP3 branch includes the 4 KiB WAV buffer, reverse in-place mono expansion,
  short-write retries and deferred volume-config/card-size operations during
  active or paused playback. The fixed 512-byte/2 ms active-player pacing is
  removed. Status now reports maximum WAV SD-read/I2S-write time and short writes.
- MP3 decoding applies per-service output backpressure rather than letting the
  decoder consume a whole track while holding the shared playback mutex. STOP
  discards queued PCM; only natural EOF drains it. Empty/invalid streams with no
  decoded audio report failure. The audio task has an 8 KiB internal-RAM stack.

The Director still selects and operates shows. Nodes execute; they do not select
scenes. P4/Emergency protections do not depend on the Director being available.
This merge does not change that control model or permit automatic restart.
The P4 change is limited to its version label and the two existing audio-path
validation call sites. Its pixel code and wiring are unchanged.

## Validation and practical limits

All 16 existing PowerShell host suite groups passed. Director/Audio CJS and
Studio JavaScript regressions passed. Added/adapted tests execute actual playback,
codec, storage-loop and MP3-adapter code against mocked I/O. They cover partial
writes/sample order, mono expansion, consecutive eight-second gaps, normal
loops, immediate first playback, pause/stop/error handling, deferred SD writes,
bounded MP3 output, natural EOF drain and stop-time discard. Decoder/I2S mocks
are not a real libmad decode or a physical continuity/response-time test.

The original 22-second non-DMX fixture is unchanged and still uses its WAV cue.
No DMX, OTA, release or automatic device flashing is introduced.

## Physical evidence and next bench

The owner confirmed audible dinosaur playback through the jack with **Both
outputs (external amplifier)** selected, followed by stuttering during a single
file test. Keep this known output setting for comparison. HEADPHONE-only mapping
is still an open issue, not silently declared repaired by this merge.

The owner also confirmed visible NeoPixel output on Audio Node and P4. Those
individual visual checks are marked passed in the bench result record; no exact
firmware/count/effect sequence was supplied with that confirmation. MP3 hardware
playback, stutter-free WAV/MP3, Emergency interruption, eight-second spacing and
no restart after clear remain physical checks after flashing the combined build.

Start with the same WAV to compare continuity, then a standard 128 kbps/44.1 kHz
MP3 from the Audio Node SD card. Use Director controls to play/stop, and serial
AUDIO:STATUS only for diagnostics while managed. MP3 position/bytes describe
compressed file bytes, not elapsed playback time. SD/I2S timing counters currently
measure the native WAV path; they do not establish actual DMA underflow counts.

Full target-build results are recorded below after completion.
