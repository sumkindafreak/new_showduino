# MISSION 001L — Emergency hardware acceptance procedure

**Status: AWAITING PHYSICAL BENCH TESTING.** Nothing in this document has been performed
on real boards. Expected results come from reading the firmware and from host tests.
Record what you actually observe in `docs/emergency-commissioning-results-001L.md`.

Showduino's emergency function is an **entertainment show-control interlock**. It is not
a certified emergency-stop, personnel-safety or life-safety system and must never be the
only protection for people. Nothing in this procedure certifies it.

Companion documents:
* `docs/emergency-maintenance-recovery-001L.md` — what to do when storage or credentials fail.
* `docs/emergency-commissioning-results-001L.md` — the results sheet (all rows PENDING).
* `docs/emergency-station-auth-001J.md`, `docs/emergency-director-auth-001K.md` — design.

---

## 1. Firmware baseline

### 1.1 What to flash

| Item | Value |
|---|---|
| Repository / branch | `sumkindafreak/new_showduino`, `fix/emergency-persistent-latch` (PR #33, draft) |
| Last CI-passing commit before 001L | `6c5e1b7506a3c8c6c9b1d3d64799860cd1867e54` (CI run 39029560919) |
| 001L commit to test | the head of the branch named in the PR #33 "001L" section; write the full hash in the results sheet |

Firmware version strings in the source (the same strings are printed by the boards):

| Role | Sketch folder | Version string | Where it is printed |
|---|---|---|---|
| P4 Stage Engine | `firmware/stage-engine-p4/ShowduinoStageEngineP4` | `0.6.8` | `[BUILD]` line at boot, `BUILD:INFO` |
| Director (5" S3 panel) | `firmware/director-esp32-8048s050/ShowduinoDirector8048S050` | `0.9.11-director` | `[BUILD]` line at boot, `BUILD:INFO` |
| Comms S3 | `firmware/s3-comms-controller/ShowduinoS3CommsController` | `0.5.4` | `STATUS` on its console |
| C3 Emergency Node | `firmware/c3-emergency-node/ShowduinoC3EmergencyNode` | `0.3.1` | `STATUS`, ANNOUNCE |

The version strings were not changed by 001J–001L, so **the commit hash is the only
reliable identifier**. Record it for every board.

### 1.2 Toolchain used by CI (assumptions, not confirmed bench settings)

| Item | CI value |
|---|---|
| arduino-cli | 1.1.1 |
| ESP32 Arduino core | `esp32:esp32` 3.3.11 |
| P4 FQBN | `esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3` (default partition: app slot 1,310,720 B) |
| C3 FQBN | `esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio`; libs Adafruit NeoPixel, Adafruit SSD1306, Adafruit GFX |
| Comms S3 FQBN | `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB` |
| Director FQBN | `esp32:esp32:esp32s3:CDCOnBoot=default,USBMode=default,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB` |
| Director libraries | `lvgl@9.2.2`, "GFX Library for Arduino" (latest at build time), `TAMC_GT911`, Adafruit NeoPixel; `lv_conf.h` copied next to the libraries folder |

**The real Director partition scheme and LVGL version are not recorded anywhere in the
repository.** The CI values above are guesses that make the build pass. They are not
confirmed hardware settings. To establish them:

1. In the Arduino IDE you normally flash the Director from, write down *Tools → Partition
   Scheme*, *Flash Size*, *PSRAM*, *USB CDC On Boot*, and the installed versions of
   **lvgl**, **GFX Library for Arduino** and **TAMC_GT911** (*Library Manager*), and the
   **esp32** core version (*Boards Manager*).
2. Flash the 001L Director firmware with those settings and read the boot line
   `[BUILD] role=DIRECTOR ... lvgl=x.y.z` and the next `[BUILD] flash=... partition=...
   size=...` line (or type `BUILD:INFO`). `size` is the running app partition size.
3. Copy both into the results sheet (row L-A01). If they differ from CI, the CI size
   figures for the Director do not describe the bench build.

Do the same for the P4 (`BUILD:INFO` on its USB console).

### 1.3 Reproducible build of the CI-passing firmware

```
git fetch origin fix/emergency-persistent-latch
git checkout 6c5e1b7506a3c8c6c9b1d3d64799860cd1867e54      # or the 001L commit
arduino-cli core install esp32:esp32@3.3.11
arduino-cli compile --fqbn "esp32:esp32:esp32p4:PSRAM=enabled,FlashSize=16M,ChipVariant=postv3" \
  firmware/stage-engine-p4/ShowduinoStageEngineP4
arduino-cli lib install "Adafruit NeoPixel" "Adafruit SSD1306" "Adafruit GFX Library"
arduino-cli compile --fqbn "esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio" \
  firmware/c3-emergency-node/ShowduinoC3EmergencyNode
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi,PartitionScheme=default_8MB" \
  firmware/s3-comms-controller/ShowduinoS3CommsController
arduino-cli lib install "lvgl@9.2.2" "GFX Library for Arduino" "TAMC_GT911"
cp firmware/director-esp32-8048s050/ShowduinoDirector8048S050/lv_conf.h \
   "$(arduino-cli config get directories.user)/libraries/lv_conf.h"
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=default,USBMode=default,FlashSize=16M,FlashMode=qio,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  firmware/director-esp32-8048s050/ShowduinoDirector8048S050
```

The same commands run in `.github/workflows/p4-arduino-build.yml` on every push. "GFX
Library for Arduino" is not pinned, so a later rebuild can pull a different version;
pin it once the bench version is known. CI cannot publish its binaries from this
environment, so keep your own `.bin` files (Arduino IDE: *Sketch → Export Compiled
Binary*) with the commit hash in the file name.

---

## 2. Equipment checklist

Tick only what is physically on the bench. **Do not assume any peripheral is connected.**

| ✓ | Item | Needed for |
|---|---|---|
| ☐ | P4 Stage Engine (Waveshare ESP32-P4 board) with SD card containing `emergency.wav` | all |
| ☐ | Comms ESP32-S3, UART-wired to the P4 | all wireless tests |
| ☐ | Director 5" ESP32-S3 touch panel | A, D |
| ☐ | At least one C3 Emergency Node (two for the "missing station" tests) | A, B, C, D |
| ☐ | Momentary push button (normally open) for P4 GPIO25 | A, C, D |
| ☐ | One computer with 4 serial terminals at 115200 baud (P4, Director, S3, C3) | all |
| ☐ | Low-voltage indicator for P4 IO1 (LED + 330 Ω to GND, or the analyser input only) | A, E, timing |
| ☐ | Short WS2812 strips (≤ 10 LEDs) for GPIO23 and GPIO24, 5 V supply sized for them | A, E |
| ☐ | Optional nodes actually connected: Audio Node ☐ Pixel Node ☐ Lamp Node ☐ MOSFET Node ☐ | E |
| ☐ | Logic analyser (≥ 1 MHz sample rate, 3.3 V inputs) or 2-channel oscilloscope | timing (§8) |
| ☐ | Small speaker or headphones on the P4 audio output, at low volume | A, E |
| ☐ | Phone or camera for screenshots/video | evidence |
| ☐ | Bench power supplies / USB hub able to power every board at once | all |

---

## 3. Precautions and safe isolation (read before powering anything)

1. **Disconnect all mains loads.** No mains lamps, relays switching mains, fog machines,
   motors, pneumatics, drop mechanisms or other moving or hazardous equipment may be
   connected to any node during these tests.
2. MOSFET Node and relay outputs: replace every load with an LED + resistor or a
   multimeter. Do not test with the real prop attached.
3. P4 IO1/IO2 (GPIO46/47) are 3.3 V logic pins: indicator LED with series resistor or the
   analyser probe only. No external voltage on them.
4. GPIO48 (timing probe, §8) is for the analyser input only. No load.
5. LED strips: power them from their own 5 V supply; join its GND to the board GND.
   Keep strips short so the supply is never overloaded at full white (emergency forces
   full-bright white).
6. Audio: keep amplifier volume low. When measuring audio with a scope, many small
   class-D amplifiers have **bridged outputs (neither speaker wire is ground)**: never
   connect a scope ground clip to a speaker terminal; probe the line/headphone output
   instead, or use a differential probe.
7. Logic analyser: connect its GND to the P4 GND. When probing two boards (C3 and P4),
   power both from the same USB hub or join their grounds; do not connect grounds of
   boards fed from different mains-referenced supplies without checking.
8. Never pull power from a board in the middle of a provisioning command or a clear
   (watch the console line finish first). Controlled power interruption is described in
   test C-04 only.
9. Keep a written note of every key fingerprint you create. Never write the key itself
   down or photograph it. Clear terminal scroll-back after provisioning.

---

## 4. Pin and wiring reference (from the firmware source)

"Verified" says whether the source itself marks the pin as checked on hardware. Confirm
every row on your board before wiring; the results sheet has a row for this.

| Board | Signal | GPIO | Source | Verified in source |
|---|---|---|---|---|
| P4 | Physical emergency button (NO to GND, internal pull-up) | 25 | `BoardConfig.h` `SHOWDUINO_ESTOP_GPIO` | not stated — confirm |
| P4 | UART RX ← S3 TX | 4 | `BoardConfig.h` | not stated |
| P4 | UART TX → S3 RX | 5 | `BoardConfig.h` | not stated |
| P4 | Emergency/signage pixel line | 24 | `BoardConfig.h` | not stated |
| P4 | Show pixel line | 23 | `BoardConfig.h` | not stated |
| P4 | Generic IO1 / IO2 (3.3 V logic) | 46 / 47 | `BoardConfig.h` | not stated |
| P4 | Timing probe (001L, disabled unless built with `-DSHOWDUINO_TIMING_PROBE_PIN=48`) | 48 | `BoardConfig.h` | reserved, unused |
| P4 | Status LED | none (`-1`) | `BoardConfig.h` | — |
| Comms S3 | UART RX ← P4 TX | 18 | `BoardConfig.h` | not stated |
| Comms S3 | UART TX → P4 RX | 17 | `BoardConfig.h` | not stated |
| Comms S3 | Status RGB | 48 | `BoardConfig.h` | not stated |
| C3 Emergency Node | Emergency button (active LOW, pull-up) | 4 | `BoardConfig.h` | **No** (`GPIO_VERIFIED 0`, "physically unverified") |
| C3 Emergency Node | Maintenance re-arm (BOOT) | 9 | `BoardConfig.h` | No |
| C3 Emergency Node | Pixel data | 2 | `BoardConfig.h` | No |
| C3 Emergency Node | OLED SDA / SCL | 5 / 6 | `BoardConfig.h` | No |
| Audio Node (A1S) | PA enable / status pixel / HP detect / SD detect | 21 / 22 / 39 / 34 | `BoardConfig.h` | not stated |
| Pixel Node (C3) | Pixel data | 2 | `BoardConfig.h` | not stated |
| Lamp Node (S3) | Pixel / mic / light / volt / motion | 8 / 4 / 5 / 6 / 15 | `BoardConfig.h` (`PHYSICAL_CONFIRMED` set) | yes, except motion (`MOTION_PIN_CONFIRMED 0`) |
| MOSFET Node | OUT1–OUT4 / status LED / identifier pixel | 16, 17, 26, 27 / 23 / 25 | `BoardConfig.h` | **No** (`GPIO_VERIFIED 0`) |

UART link: P4 GPIO4 ← S3 GPIO17, P4 GPIO5 → S3 GPIO18, common GND, 115200 baud.

**Director → Comms MAC is hard-coded** (`BoardConfig.h`, `SHOWDUINO_COMMS_MAC_0..5` =
`20:6E:F1:98:B5:38`). Type `MAC` on the Comms S3 console; if it differs, edit those six
values and reflash the Director before any test, or the Director cannot reach the S3.

---

## 5. Serial consoles

All four boards: 115200 baud, newline line ending. Keep a log file per board for the
whole session (most terminal programs can log to file). Name them
`<date>_<board>_<commit7>.log`.

Commands confirmed in the 001L firmware (only these are used below):

| Board | Commands |
|---|---|
| P4 | `HELP`, `BUILD:INFO`, `STATUS:REQUEST`, `LOOP:STATS`, `LOOP:RESET`, `EMERGENCY:STOP`, `EMERGENCY:CLEAR`, `EMERGENCY:CLEAR_CANCEL`, `EMERGENCY:AUTH:STATUS`, `EMERGENCY:KEY:SET:ESTOP-nn:<hex>`, `EMERGENCY:KEY:LIST`, `EMERGENCY:KEY:REVOKE:ESTOP-nn`, `EMERGENCY:DIRKEY:SET:DIR-nn:<hex>`, `EMERGENCY:DIRKEY:STATUS`, `EMERGENCY:DIRKEY:REVOKE`, `EMERGENCY:RECOVER:STATUS`, `EMERGENCY:RECOVER:BLOCKERS:<confirm>`, `EMERGENCY:RECOVER:COUNTER:<confirm>`, `IO:1:MODE:OUTPUT`, `IO:1:ACTIVE:HIGH`, `IO:1:ON`, `IO:1:STATUS`, `IO:STATUS`, `PIXEL:STATUS`, `SHOW:START` |
| Director | `BUILD:INFO`, `PROVISION DIRECTOR NEW DIR-nn`, `PROVISION DIRECTOR STATUS`, `PROVISION DIRECTOR CLEAR`, `HELP`; any other line is forwarded to the P4 as plain text |
| Comms S3 | `HELP`, `STATUS`, `MAC`, `PING:P4` |
| C3 Emergency Node | `HELP`, `STATUS`, `MAC`, `PINS`, `PROVISION STATUS`, `PROVISION NEW ESTOP-nn`, `PROVISION SET ESTOP-nn <hex>`, `PROVISION CLEAR` |

---

## 6. Provisioning sequence (do once, in this order)

Flash order: **Comms S3 → P4 → C3 node(s) → Director**, all from the same commit. All
boards must run 001K-or-later firmware; an older C3 cancels every Director clear.

1. **P4 boot check.** Power the P4 alone. Expect `[BUILD] role=P4 fw=0.6.8 ...` and
   `[ESTOP] Stored latch CLEAR` on a fresh board. Type `EMERGENCY:AUTH:STATUS`: note
   `nonce=seeded`.
2. **S3 link.** Power the S3. On the S3 console `PING:P4` → expect a PONG line. On the P4
   console `[COMMS] UART RX: DIAG:PING`.
3. **Station key (per C3).** On the C3 console (button released):
   `PROVISION NEW ESTOP-01`. It prints `NEW KEY for ESTOP-01 fp=xxxxxxxx` and the line
   `EMERGENCY:KEY:SET:ESTOP-01:<64 hex>`. Paste that line into the **P4** console.
   Expect `[ESTOP] KEY:SET ESTOP-01 OK fp=xxxxxxxx` — the fingerprints must match.
4. Within a few seconds `EMERGENCY:AUTH:STATUS` on the P4 lists
   `ESTOP-01 prov=1 verified=1 online=1 pressed=0`.
5. Repeat 3–4 for ESTOP-02 (needed for tests B-06, D-06).
6. **Director key.** On the Director console: `PROVISION DIRECTOR NEW DIR-01`. Paste the
   printed `EMERGENCY:DIRKEY:SET:DIR-01:<hex>` line into the **P4** console. Expect
   `DIRKEY:SET DIR-01 OK fp=...` with no `RESTRICTED` (the P4 must not be latched now).
   Fingerprints must match `PROVISION DIRECTOR STATUS`.
7. A key that equals any existing station or Director key is refused (`FAILED`). This
   can only happen if a key was pasted twice; generate a new one.
8. Clear all terminal scroll-back. Record only IDs and fingerprints.

---

## 7. Tests (run in this order)

For every test record: date/time, commit, board console excerpts, photo/video where an
output is visual, and the result in the results sheet. **Expected** below is what the
firmware is written to do. If you see anything else, mark FAIL and keep the logs.

Prepare once before section A (P4 console):
```
IO:1:MODE:OUTPUT
IO:1:ACTIVE:HIGH
```
IO1 then drives the indicator LED (§3.3). `IO:1:ON` lights it.

### A — Basic functionality

| ID | Steps | Expected | Pass if |
|---|---|---|---|
| A-01 | Power all four core boards. On P4 and Director type `BUILD:INFO`. On S3 `STATUS`, on C3 `STATUS`. | Each board boots without reset loops; `[BUILD]` lines show the flashed commit's settings | All four respond; versions recorded |
| A-02 | Director: `PROVISION DIRECTOR STATUS`. P4: `EMERGENCY:DIRKEY:STATUS` | enrolled, same fingerprint, `restricted=0` | fingerprints equal |
| A-03 | P4: `EMERGENCY:AUTH:STATUS` | ESTOP-01 `prov=1 verified=1 online=1` | as expected |
| A-04 | Leave idle 5 min, repeat A-03 | station stays online/verified | no OFFLINE |
| A-05 | `IO:1:ON` (LED lit). Press the P4 GPIO25 button and release | Emergency latches at once; LED goes off; GPIO24/23 strips go full white; P4 plays `emergency.wav`; Director shows Emergency; console `[ESTOP] EMERGENCY ACTIVE` | all observed |
| A-06 | While latched: `IO:1:ON`, `PIXEL:SOLID:255:0:0`, `SHOW:START` | LED stays off; strips stay white; `SHOW:START:REJECTED:EMERGENCY` | no output leaves the emergency state |
| A-07 | Clear: on the Director touch E-CLEAR, then Confirm in the dialog | P4 prints `EMERGENCY:CLEAR:VERIFYING`, then `EMERGENCY:CLEAR:OK` and `[ESTOP-TIMING] clear decision=ACCEPTED elapsed_ms=...`; strips back to normal; LED stays **off** (outputs never resume) | cleared; LED off |
| A-08 | Press and release the C3 button | P4 latches (same outputs as A-05); C3 `STATUS` shows `latch=1 ack=1` | latched |
| A-09 | Director E-CLEAR + Confirm with the C3 released | accepted; C3 returns to NORMAL | accepted only after C3 proves released |
| A-10 | Latch again (`EMERGENCY:STOP` on P4). Clear from the P4 console: `EMERGENCY:CLEAR` | verification then `CLEAR:OK` | accepted |

### B — Wireless failures

| ID | Steps | Expected | Pass if |
|---|---|---|---|
| B-01 | Latch (C3 press/release). Unplug the Comms S3. Try a Director clear, then `EMERGENCY:CLEAR` on the P4 | Director cannot reach the P4; USB clear refused with an `EMERGENCY:CLEAR:REJECTED:` line (`WIRELESS_ASSERTED` while ESTOP-01 is offline, or `STATION_UNVERIFIED:ESTOP-01` if no proof arrives) | latch kept |
| B-02 | Reconnect the S3; wait 10 s; clear from Director | accepted | accepted after link returns |
| B-03 | Latch. Power off the C3. Try to clear | refused (`WIRELESS_ASSERTED` or `STATION_UNVERIFIED:ESTOP-01`) | latch kept |
| B-04 | Power the C3 back on (button released); wait for `online=1`; clear | accepted | accepted |
| B-05 | Latch. Carry the C3 out of range (or shield it in a metal tin) ≥ 15 s; try to clear; bring back | refused while away; accepted after return and fresh proof | as stated |
| B-06 | Provision ESTOP-02, then power it off permanently. Latch from ESTOP-01, release, clear | refused (`WIRELESS_ASSERTED` or `STATION_UNVERIFIED:ESTOP-02`) | missing station blocks |
| B-07 | (Advanced, optional) Replay: capture ESP-NOW frames with a sniffer and re-send them | no state change | only if the tool exists; otherwise mark NOT PERFORMED (host tests S4–S8 cover it) |

### C — Restart and persistence

Wait at least **10 seconds after the last emergency state change** before removing power
in any C test, so no flash write is in progress. Do not erase or edit NVS.

| ID | Steps | Expected | Pass if |
|---|---|---|---|
| C-01 | Latch (GPIO25). Release. Wait 10 s. Power-cycle the P4 | Boot prints `Stored latch ACTIVE (blockers verified) — Emergency restored`; outputs white/off again before the show can run; Director shows Emergency | restored latched |
| C-02 | Latch from ESTOP-01, keep the C3 **pressed**, power-cycle the P4 | latched after boot; clear refused (`WIRELESS_ASSERTED` or `STATION_PRESSED:ESTOP-01`) until released | blocker restored |
| C-03 | Latched. Power-cycle the Director. Then between E-CLEAR and Confirm power-cycle it again | nothing clears; the open request expires | still latched |
| C-04 | Latched. Power-cycle the C3 | C3 shows its button state; P4 stays latched; after release the clear works | no unintended clear |
| C-05 | Note `EMERGENCY:AUTH:STATUS` `episode=` and `EMERGENCY:DIRKEY:STATUS` `lastCounter=`. Reboot P4, Director and C3 (one at a time, latched and unlatched) and do one clear each time | `lastCounter` never goes down; episode numbers never repeat | counters survive |
| C-06 | Controlled power interruption: latched and idle 10 s, remove P4 power for 5 s, restore. Repeat 10 times | latched every time | 10/10 latched |
| C-07 | Review C-01…C-06 logs | no boot ever printed `Stored latch CLEAR` after an un-cleared emergency | none |
| C-08 | Non-destructive recovery-command check: while latched with a healthy store, P4 `EMERGENCY:RECOVER:STATUS`, then `EMERGENCY:RECOVER:BLOCKERS:INSPECTED-ALL-STATIONS` | status shows `unknownBoot=0`; command replies `RECOVER:BLOCKERS REFUSED_NOT_NEEDED — emergency stays latched` | refused, still latched |
| C-09 | Type the same `EMERGENCY:RECOVER:BLOCKERS:...` line on the **Director** console (it is forwarded over the S3) | P4: `REJECTED station maintenance command — local USB console only` | refused |

Lost or corrupted records are **not** tested on the bench: there is no approved,
recoverable way to damage a production record. They are covered by host tests (L1, L7)
and by `docs/emergency-maintenance-recovery-001L.md`.

### D — Director security

| ID | Steps | Expected | Pass if |
|---|---|---|---|
| D-01 | Normal signed clear (A-07) | accepted | — |
| D-02 | Latch. On the **Director console** type `EMERGENCY:CLEAR`, then `EMERGENCY:CLEAR_CONFIRM` (forwarded unsigned) | P4 `EMERGENCY:CLEAR:REJECTED:AUTH_REQUIRED` both times | refused |
| D-03 | (Advanced, optional) replay a captured `DCLR:` line into P4 GPIO4 with a 3.3 V USB-UART adapter in place of the S3 | `REPLAY`, `WRONG_EPISODE` or `STALE_TRANSACTION` | refused; or NOT PERFORMED |
| D-04 | Wrong credential: on the P4 enter `EMERGENCY:DIRKEY:SET:DIR-01:<a different key, e.g. from openssl rand -hex 32>`. Try a Director clear. Afterwards re-enrol properly (§6.6) | `AUTH_FAILED` | refused |
| D-05 | Latch. Hold GPIO25. Director E-CLEAR | `BUTTON_ACTIVE`; latched | refused |
| D-06 | ESTOP-02 off (B-06). Director clear | refused (`WIRELESS_ASSERTED` or `STATION_UNVERIFIED:ESTOP-02`) | refused |
| D-07 | For D-02…D-06 photograph the Director screen | a readable refusal reason (station ID where relevant) | operator can tell why |
| D-08 | Enrol a Director while latched | `RESTRICTED`; its clear refused `DIRECTOR_RESTRICTED`; P4 USB clear works and lifts the restriction | as stated |

### E — Multi-node emergency behaviour (only nodes physically connected)

Mark NOT CONNECTED for any node you do not have. Expected behaviour is from the node
firmware; none of it has been seen on hardware.

| ID | Steps | Expected (from code) | Pass if |
|---|---|---|---|
| E-01 | Audio Node playing a show track; latch | track stops; output muted; PA off | silence within your measured time (§8 T-05) |
| E-02 | P4 local audio | `emergency.wav` plays on the P4 output (missing file is reported `audio_wav=missing`) | emergency audio heard |
| E-03 | Pixel Node running an effect; latch | whole line full white | white |
| E-04 | Lamp Node on; latch | emergency state (white) | white |
| E-05 | MOSFET Node outputs on (LED loads); latch | all four OUT off; identifier pixels white | all off |
| E-06 | While latched, send show cues/commands to each node (Studio or Director) | no node leaves the emergency state | none overrides |
| E-07 | Latched; unplug the Comms S3 for 15 s | nodes already in emergency stay so; nodes that missed the emergency fall back to safe (lamp off, audio stop, pixel blackout, MOSFET off) after about 8 s **only if show-controlled** — this is not the white emergency state | record per node |
| E-08 | Show running; remove P4 power while nodes stay powered | record what each node does and how long it takes | no node keeps a hazardous output; record times |

---

## 8. Timing measurement (§3 of the brief)

Never estimate these values. Measure them, repeat each at least 20 times, record every
sample, and report minimum, median and **worst observed**. These are bench measurements
of one installation, not a safety rating.

### 8.1 Timing build

Rebuild the P4 with the probe enabled, e.g. in Arduino IDE add to `BoardConfig.h`
temporarily, or with arduino-cli:
`--build-property "compiler.cpp.extra_flags=-DSHOWDUINO_TIMING_PROBE_PIN=48"`.
Boot line must show `timing_probe=48`. GPIO48 goes HIGH as the first action of a new
emergency activation and LOW when a clear is applied. Use the normal build for every
other test.

### 8.2 Channels

| Ch | Signal | Point |
|---|---|---|
| 1 | P4 GPIO25 (button) | button pin |
| 2 | P4 GPIO48 (probe) | header |
| 3 | P4 IO1 GPIO46, configured OUTPUT and ON before the trigger | LED anode side |
| 4 | P4 GPIO24 emergency pixel data | strip DIN |
| 5 | C3 GPIO4 (button) | C3 button pin (common GND!) |
| 6 | Audio output (scope, see §3.6) | line/headphone out of P4 or Audio Node |

### 8.3 Intervals

| ID | Interval | Start edge | End edge | Notes |
|---|---|---|---|---|
| T-01 | Physical input → P4 detection | Ch1 falling | Ch2 rising | includes the 30 ms debounce and up to about two main-loop periods (by design) |
| T-02 | Detection → first local output | Ch2 rising | Ch3 falling | also note Ch4 first data burst |
| T-03 | C3 press → P4 receipt | Ch5 falling | Ch2 rising | includes C3 debounce, ESP-NOW, S3 relay, UART, P4 loop |
| T-04 | P4 receipt → local output (wireless source) | Ch2 rising | Ch3 falling | same as T-02 but from a C3 press |
| T-05 | Emergency output → audio response | Ch3 falling | Ch6 show audio stops / emergency audio starts | do once for P4 audio, once per Audio Node |
| T-06 | Director confirm → decision | P4 receipt of the confirm | decision | read `[ESTOP-TIMING] clear decision=... elapsed_ms=N` on the P4 console. Measures P4 receipt to decision (includes station verification, ≤ 1000 ms window). Touch-to-receipt is not instrumented |

### 8.4 Main-loop timing

T-01 depends on the slowest P4 loop iterations. Measure them under each condition:
`LOOP:RESET`, run the condition for 10 minutes, then `LOOP:STATS`. Record the count,
average, max and the histogram (`<=1ms … >100ms`).

| Condition | |
|---|---|
| L-1 | idle, no show |
| L-2 | show running with audio and pixel effects |
| L-3 | emergency latched (white pixels, emergency audio) |
| L-4 | Director and 2 stations connected, repeated clear attempts |
| L-5 | SD card removed / reinserted during idle |

Any loop above 100 ms is a finding: note what was happening and report it.

### 8.5 What the code inspection found (001L, not measured)

* P4 GPIO25 is polled from `loop()`, not by interrupt. Detection time = debounce (30 ms)
  + loop jitter. The loop also services SD, audio, Ethernet/E1.31, pixels and the UART.
* 001L changes: the engage path now drives generic outputs, emergency pixels, show
  pixels and the status LED **before** any serial print or SD log; the restored-at-boot
  path does the same; a wireless ASSERT is logged after actuation; the C3 transmits its
  ASSERT before logging it. No measured benefit is claimed.
* `SD_LOG*` on the P4 are serial prints, not SD writes. Serial output can still block
  when the USB buffer is full; outputs no longer wait on it.
* NVS writes (latch, blockers) run after the outputs (001F engage order).
* Not changed: node propagation goes over UART → S3 → ESP-NOW after the local outputs;
  audio start depends on the SD file and codec.

---

## 9. Troubleshooting

| Symptom | Check |
|---|---|
| Director shows no Stage / cannot clear | Comms MAC (§4); S3 `PING:P4`; UART wires crossed (TX→RX) and common GND |
| `KEY:SET ... FAILED` | wrong length key; key equal to another key; P4 storage fault (`EMERGENCY:AUTH:STATUS keyFault=1`) |
| Station never `verified=1` | fingerprints differ; C3 on older firmware; C3 `PROVISION STATUS` shows `provisioned=no`; Wi-Fi channel (both use channel 1) |
| Clear refused `WIRELESS_ASSERTED` or `STATION_UNVERIFIED:<id>` | that station off, out of range, or the S3 is down. Bring it back, or follow the recovery document to revoke/dismiss it after physical inspection |
| Clear refused `STATION_STORAGE` | `EMERGENCY:RECOVER:STATUS` — see the recovery document |
| Clear refused `AUTH_FAILED` / `DIRECTOR_NOT_ENROLLED` | re-enrol the Director (§6.6) |
| Clear refused `DIRECTOR_RESTRICTED` | Director was enrolled during this emergency; clear on the P4 USB console |
| Boot prints `latch storage unreadable, corrupt or lost — fail-safe LATCHED` | stop; follow `docs/emergency-maintenance-recovery-001L.md` |
| Director boot loops / `FATAL: Enable OPI PSRAM` | Arduino IDE board settings (§1.2) |

---

## 10. Remaining security issues (001L reassessment)

| Issue | Status after 001L | Recommendation |
|---|---|---|
| **J-14** hostile S3 hides a press | Unchanged. The guarantee is: every provisioned station proved RELEASED in answer to a challenge issued after the authenticated confirm, at most 1000 ms before the clear is applied. A Comms S3 that is actively hostile can suppress a press made inside that window (boundary tests K16c/K16d). With an honest S3 a press re-latches within radio latency. | Accept and disclose for launch, or add an independent permissive input (below). |
| Independent physical permissive (future option) | Not designed or installed. One possibility: each station's second NC contact in a series loop into a spare P4 input that must read closed before a clear is applied. | Needs agreed hardware requirements (wiring runs, connector, input protection) before any design. |
| Credential extraction (J-08) | Station and Director keys are stored in plain NVS on the P4, Director and C3. Anyone with physical access and a USB cable can read them. | See §10.1. Operational mitigation: enclosures locked, revoke and re-key after any loss. |
| C3 SoftAP password (J-10) | Every C3 uses the published password `showduino`. The SoftAP can only change pixel settings; it cannot ACK, clear, rename a provisioned station or change its key. | See §10.2. |
| Director/station key separation | Now enforced by one shared function (`showduino_auth_key_conflicts`) used by the P4 firmware in both directions, with host tests L2a–L2f and mutants L02–L04. | — |
| Comms S3 OTA | The S3 can be updated over its web interface. Who may trigger that, and whether images are signed, was not reviewed in 001L. This is one more reason the S3 is treated as untrusted. | Review before launch. |
| P4 flash headroom | About 100 KB (7.7%) of the 1.25 MB app slot left at 6c5e1b7. | See §10.3. |

### 10.1 Flash encryption and secure boot (options, not enabled)

* **Flash encryption** (ESP32-S3, C3 and P4 support it) encrypts flash contents with a
  key held in eFuses. Enabling it burns eFuses **permanently**. In *release* mode plain
  reflashing over USB is disabled; in *development* mode it is still possible but the
  protection is weaker. Either way each board needs a one-time provisioning step, and
  the normal Arduino IDE "Upload" workflow no longer works as-is.
* **NVS encryption** protects only the key/value store. On chips with the HMAC
  peripheral (S3, C3, P4) ESP-IDF can derive the NVS key from an eFuse key. The Arduino
  core ships prebuilt with its own configuration; using this needs a custom build of the
  core libraries, which has not been tried.
* **Secure Boot v2** only lets signed firmware run. It also burns eFuses permanently; the
  signing key must be kept safe forever, and losing it means those boards can never be
  updated again.
* **Implication for off-the-shelf replacement boards:** a replacement must go through the
  same eFuse provisioning before use, by the owner, with tools beyond the Arduino IDE.
  That conflicts with "buy a board and flash it". A board provisioned wrongly cannot be
  undone.
* **Suggested path:** do not enable these for the first release. Document the physical-
  access risk, lock the enclosures, and keep the revoke/re-key procedures. Revisit
  NVS encryption on the P4 only (it holds every key) once a reproducible custom build
  exists and has been tested on a spare board.

### 10.2 C3 SoftAP credential proposal (not implemented)

1. Generate a per-device random WPA2 password (≥ 12 characters from the hardware RNG,
   which is true random while Wi-Fi runs) at first boot; store it in NVS.
2. Show it only on the C3 OLED while the maintenance button is held, and on its USB
   console (`STATUS`). Never broadcast it or send it to the P4.
3. Keep the SoftAP **off** by default; turn it on for 10 minutes after a long press of the
   maintenance button, then off again automatically.
4. `PROVISION CLEAR` also regenerates it.
This changes how owners reach the pixel page, so it needs the owner's agreement first.

### 10.3 P4 partition (reviewed, not changed)

* The P4 builds with the core's default partition table for a 16 MB P4: two
  1,310,720-byte app slots (OTA layout). The P4 firmware contains **no** OTA updater
  (`Update`/`esp_ota_begin` are not used), so the second slot is unused today.
* A larger single app slot would free space but moves partition offsets. If the NVS
  partition moves, the latch, blocker set, counters, station keys and Director key are
  lost. That loss is fail-safe only for the latch byte that has a marker (001L); keys
  must then be re-entered for every station and the Director.
* Migration plan required before any change: (1) clear any emergency normally;
  (2) record IDs and fingerprints; (3) back up a full flash image with `esptool
  read_flash`; (4) flash bootloader + new partition table + app over USB; (5) re-provision
  every station and the Director; (6) run section A and C again. Not done in 001L.

---

## 11. Evidence capture

For every test row in the results sheet fill:
* **Actual** — what you saw, in your own words, plus the exact console line(s).
* **Evidence** — log file name and line numbers, photo/video file name, analyser capture
  file name.
* **Status** — PASS / FAIL / NOT PERFORMED / NOT CONNECTED. Leave PENDING until done.
