# MISSION 001L — Emergency commissioning results

**Acceptance status: AWAITING PHYSICAL BENCH TESTING.**
No row below has been performed. Every **Actual** field says "Not yet tested" and every
**Status** is PENDING. Only the person performing a test on real hardware may change
them, and only with evidence (log excerpt, photo, capture file).

Procedure: `docs/emergency-hardware-acceptance-001L.md`. Recovery:
`docs/emergency-maintenance-recovery-001L.md`.

Status values: PENDING · PASS · FAIL · NOT PERFORMED (state why) · NOT CONNECTED.

## Session record

| Field | Value |
|---|---|
| Date(s) | |
| Tester | |
| Witness (optional) | |
| Commit hash flashed (full) | |
| P4 `BUILD:INFO` lines | |
| Director `BUILD:INFO` lines | |
| Director Arduino IDE settings (partition, flash, PSRAM, CDC on boot) | |
| Director libraries (lvgl, GFX Library for Arduino, TAMC_GT911) versions | |
| ESP32 core version used for flashing | |
| Comms S3 `STATUS` / `MAC` | |
| C3 nodes: ID, MAC, key fingerprint (never the key) | |
| Director ID and key fingerprint | |
| Nodes connected (Audio / Pixel / Lamp / MOSFET) | |
| Loads isolated (confirm, initials) | |

## Results matrix

| Test ID | Test | Expected | Actual | Status |
|---|---|---|---|---|
| L-A01 | Firmware baseline recorded (§1) | Commit, `BUILD:INFO`, IDE settings and library versions recorded for P4 and Director | Not yet tested | PENDING |
| L-A02 | Pin map confirmed against hardware (§4) | Every wired pin matches the table; differences noted | Not yet tested | PENDING |
| L-A03 | Director → Comms MAC matches (§4) | Director reaches the S3 | Not yet tested | PENDING |
| A-01 | Boot all four core roles | No reset loops; versions reported | Not yet tested | PENDING |
| A-02 | Director enrolled | Matching fingerprints, `restricted=0` | Not yet tested | PENDING |
| A-03 | C3 provisioned and authenticated | `prov=1 verified=1 online=1` | Not yet tested | PENDING |
| A-04 | Station freshness over 5 min | Stays online/verified | Not yet tested | PENDING |
| A-05 | P4 GPIO25 press | Latches; IO1 off; strips white; emergency audio; Director shows Emergency | Not yet tested | PENDING |
| A-06 | Output override while latched | `IO:1:ON`, pixel and show commands do not leave emergency state; `SHOW:START:REJECTED:EMERGENCY` | Not yet tested | PENDING |
| A-07 | Director signed clear | `CLEAR:VERIFYING` → `CLEAR:OK`; IO1 stays off | Not yet tested | PENDING |
| A-08 | C3 button press | P4 latches; C3 `latch=1 ack=1` | Not yet tested | PENDING |
| A-09 | Clear with released station | Accepted after fresh proof; C3 returns to NORMAL | Not yet tested | PENDING |
| A-10 | P4 USB maintenance clear | Verification then `CLEAR:OK` | Not yet tested | PENDING |
| B-01 | Comms S3 disconnected while latched | Clear refused; latch kept | Not yet tested | PENDING |
| B-02 | Comms S3 restored | Clear accepted | Not yet tested | PENDING |
| B-03 | C3 powered off while latched | Clear refused | Not yet tested | PENDING |
| B-04 | C3 restored | Clear accepted after proof | Not yet tested | PENDING |
| B-05 | C3 out of range | Refused while away; accepted after return | Not yet tested | PENDING |
| B-06 | Missing provisioned station | Clear refused | Not yet tested | PENDING |
| B-07 | Stale / replayed station packets (optional tool) | No state change | Not yet tested | PENDING |
| C-01 | P4 reboot while latched | Latch restored; outputs re-engaged | Not yet tested | PENDING |
| C-02 | P4 reboot with a pressed station | Blocker restored; clear refused until released | Not yet tested | PENDING |
| C-03 | Director reboot (latched / mid-dialog) | Nothing clears; request expires | Not yet tested | PENDING |
| C-04 | C3 reboot while latched | P4 stays latched | Not yet tested | PENDING |
| C-05 | Replay counters across reboots | `lastCounter` never decreases; episodes never repeat | Not yet tested | PENDING |
| C-06 | Controlled power interruption ×10 | Latched every time | Not yet tested | PENDING |
| C-07 | No restart cleared the system | No unexpected `Stored latch CLEAR` | Not yet tested | PENDING |
| C-08 | `RECOVER:BLOCKERS` on a healthy store | `REFUSED_NOT_NEEDED`; still latched | Not yet tested | PENDING |
| C-09 | Recovery command via Director/S3 | `local USB console only` | Not yet tested | PENDING |
| D-01 | Authenticated request and confirm | Accepted | Not yet tested | PENDING |
| D-02 | Unsigned clear commands | `REJECTED:AUTH_REQUIRED` | Not yet tested | PENDING |
| D-03 | Replayed confirmation (optional tool) | `REPLAY` / `WRONG_EPISODE` / `STALE_TRANSACTION` | Not yet tested | PENDING |
| D-04 | Invalid Director credential | `AUTH_FAILED` | Not yet tested | PENDING |
| D-05 | Clear with GPIO25 held | `BUTTON_ACTIVE` | Not yet tested | PENDING |
| D-06 | Clear with a missing station | Refused | Not yet tested | PENDING |
| D-07 | Director shows refusal reasons | Readable reason on screen | Not yet tested | PENDING |
| D-08 | Director enrolled while latched | Restricted; USB clear lifts it | Not yet tested | PENDING |
| E-01 | Audio Node show audio stops | Stop, mute, PA off | Not yet tested | PENDING |
| E-02 | Emergency audio selected | `emergency.wav` on P4 output | Not yet tested | PENDING |
| E-03 | Pixel Node override | Whole line white | Not yet tested | PENDING |
| E-04 | Lamp Node emergency state | White | Not yet tested | PENDING |
| E-05 | MOSFET Node interlock | All outputs off; identifier pixels white | Not yet tested | PENDING |
| E-06 | Cues cannot override emergency | No node leaves emergency state | Not yet tested | PENDING |
| E-07 | Comms S3 outage during emergency | Per-node behaviour recorded | Not yet tested | PENDING |
| E-08 | P4 power loss with nodes powered | No hazardous output; times recorded | Not yet tested | PENDING |

## Timing results (fill only with measured values)

Each row: at least 20 samples. Do not enter estimates.

| ID | Interval | Samples | Min (ms) | Median (ms) | Worst observed (ms) | Capture files | Status |
|---|---|---|---|---|---|---|---|
| T-01 | GPIO25 input → P4 detection | | | | | | PENDING |
| T-02 | P4 detection → first local output | | | | | | PENDING |
| T-03 | C3 press → P4 receipt | | | | | | PENDING |
| T-04 | P4 receipt → local output (wireless) | | | | | | PENDING |
| T-05 | Emergency output → audio response | | | | | | PENDING |
| T-06 | Director confirm received → decision (`elapsed_ms`) | | | | | | PENDING |

| Loop condition | count | avg µs | max µs | ≤1 | ≤5 | ≤20 | ≤50 | ≤100 | >100 ms | Status |
|---|---|---|---|---|---|---|---|---|---|---|
| L-1 idle | | | | | | | | | | PENDING |
| L-2 show running | | | | | | | | | | PENDING |
| L-3 emergency latched | | | | | | | | | | PENDING |
| L-4 Director + stations, clear attempts | | | | | | | | | | PENDING |
| L-5 SD removed / reinserted | | | | | | | | | | PENDING |

## Findings during commissioning

| # | Test ID | Observation | Log / evidence | Severity | Follow-up |
|---|---|---|---|---|---|
| | | | | | |

## Open blockers at the time of writing (not test results)

* No hardware acceptance performed (this sheet).
* J-14: a hostile Comms S3 can hide a station press inside the ≤ 1000 ms verification
  window. Disclosed residual.
* Station and Director keys stored without flash or NVS encryption (J-08).
* C3 SoftAP published default password (J-10); proposal only.
* P4 flash headroom about 100 KB; no partition change made.
* NVS fault behaviour not observed on hardware (recovery doc §6).
* No safety certification. Not life-safety equipment.

## Sign-off

The emergency subsystem may be described as hardware-accepted on this installation only
when every applicable row is PASS (or justified NOT CONNECTED / NOT PERFORMED) and every
FAIL has been fixed and re-tested.

| Role | Name | Date | Signature |
|---|---|---|---|
| Tester | | | |
| Owner | | | |
