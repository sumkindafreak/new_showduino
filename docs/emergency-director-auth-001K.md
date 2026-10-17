# MISSION 001K — authenticated Director clear authority and clearance race

Status: **implemented in firmware, host-tested, compiled in CI (see §11). Not hardware
tested. Not a certified safety function.** Sections 1–2 were written before any 001K
code change. Showduino's emergency features are show-control interlocks for
entertainment; they do not replace certified emergency-stop or life-safety equipment.

Branch `fix/emergency-persistent-latch`, PR #33 (draft, do not merge). Starting head
`b3b3f95` (001J). Not hardware tested. Not a certified safety function.

## 1. Inventory: every path that can request or confirm an emergency clear (pre-001K)

Trust chain for operator commands: Director ESP32-S3 → ESP-NOW (desk packet, 95-char
command) → Comms ESP32-S3 (untrusted relay) → UART (line ≤ 240 chars at the P4) → P4.

| # | Entry point | Code | Reaches P4 as | P4 source | Pre-001K check |
|---|---|---|---|---|---|
| E1 | Director touchscreen "E-CLEAR" (Live page, Settings page, overlay) | `ShowduinoUi.h` → `handleUiCommand("EMERGENCY:CLEAR")` → `sendToStage` | `EMERGENCY:CLEAR` | Comms | none (plain text) |
| E2 | Director clear dialog "Confirm" | `DirectorEmergencyClearDialog` → `onEmergencyClearConfirm()` → `sendToStage` | `EMERGENCY:CLEAR_CONFIRM` | Comms | none |
| E3 | Director OS2 shell command service `EmergencyClear` | `.ino` `CT::EmergencyClear` → `sendToStage("EMERGENCY:CLEAR")` | `EMERGENCY:CLEAR` | Comms | none |
| E4 | Director USB console | `handleUsbLine()` forwards any unknown line with `sendToStage` | any text | Comms | none |
| E5 | Comms S3 itself (compromised or buggy) | `commsUartWriteLine()` | any text | Comms | none — **J-05** |
| E6 | Comms S3 Web UI | `CommsWebServerBase.h` `SHOWDUINO_WEBUI_ALLOW_EMERGENCY_CLEAR` (default 0) | `CLEAR_CONFIRM` / `CLEAR_CANCEL` | Comms | compile flag only |
| E7 | P4 web API over the UART body channel | `WebApiHandlerBase.h` `webCommandAllowed()` same flag (undefined by default) | `CLEAR_CONFIRM` / `CLEAR_CANCEL` | Comms | compile flag only |
| E8 | Show timeline cues | `timelineDispatchCommand()` → `handleCommand(cmd)` | any command in a `.shdo` cue | Comms | none |
| E9 | P4 USB console | `handleCommand(..., LocalUsb)` | `EMERGENCY:CLEAR` → `clearEmergencyStop()`; `CLEAR_CONFIRM` → confirm | LocalUsb | physical access to the P4 |
| E10 | Internal | `emergencyLatchReconcile()` writes CLEAR only when the RAM latch is already clear | — | — | not an entry point |

`CommandSource` only distinguishes the P4's own USB port from everything that arrives
on the UART. It is not authentication: the Director, the S3, the S3 web UI, the P4 web
API and show cues all look identical (`Comms`).

Gates every path already had (001E–001J): the physical GPIO25 loop must be released;
the request must be pending and not older than 12 s; any new assertion supersedes it;
the 001J station clear decision (pressed/unverified/offline stations, storage, table
overflow, fresh proofs from every provisioned station).

## 2. Trust decisions for 001K

| Authority | Who | How it is proven |
|---|---|---|
| Activate emergency | anyone (GPIO25, USB, Director, stations, show) | none needed — activation is never delayed |
| Operator clear (normal) | the enrolled Director, by touch | HMAC-signed request and confirm, end to end to the P4 |
| Maintenance clear | person at the P4 USB port | physical access (documented, logged) |
| Station release | each provisioned C3 | 001J station AUTH |
| Comms S3 | nobody | relays only; cannot originate any authority |

## 3. Protocol and cryptographic design

Wire format (fits the 95-character desk packet and the S3 → P4 UART line):

```
DCLR:<REQ|CFM>:<dirId>:<episode8>:<txn16>:<counter8>:<tag32>
tag = HMAC-SHA256(directorKey, "DIRCLR|v1|<dirId>|<op>|<episode8>|<txn16>|<counter8>")[0..16)
```

| Property | Mechanism |
|---|---|
| Unique Director credential | 32-byte key per Director (`DIR-<n>`), generated on the Director (true RNG while its Wi-Fi radio runs), entered on the P4 USB console. Stored in a record separate from station keys. |
| Established primitives | HMAC-SHA256 truncated to 128 bits (mbedtls on the boards, OpenSSL in tests; same 001J helpers). Constant-time compare. |
| Forged / modified | Tag over every field; any change fails (tests D2a–D2e, K2, K3, K13). |
| Domain separation | `DIRCLR|v1` canonical prefix vs station `AUTH|v1` / `CMD|v1`; separate key record; the P4 firmware refuses a Director key equal to any station key and vice versa (001L: shared `showduino_auth_key_conflicts`, host tests L2a–L2f). D2h shows a station-format tag does not verify as a Director message. |
| Replay / duplicates / reordering | Director counter strictly increasing, block-persisted on the Director before use; P4 persists the last accepted counter **before** acting (D3, K5, K6, K9, K23). |
| Cross-reboot replay | Both counters survive restarts; transaction nonces are per boot (K6, K7). |
| Episode binding | Every message names the P4 episode (from the persisted counter); messages for another episode, or while no durable episode exists, are rejected (D4a/b, K4, K5). |
| Explicit intent per attempt | REQ = touch "E-CLEAR"; CFM = touch "Confirm" in the dialog, signed over a fresh P4 transaction nonce. A CFM is single use and only valid for the open transaction (D1g, D4c–f, K23). |
| P4 enforcement | `showduino_clear_entry()` routes every clear-related line; plain `EMERGENCY:CLEAR`, `CLEAR_REQUEST`, `CLEAR_CONFIRM` from the UART are refused `AUTH_REQUIRED` (P0, K8). No fallback. |
| No production credential in source | None. Keys exist only in the NVS of the boards they were entered on. |

P4 → Director notices (`STATE:EMERGENCY:EPISODE:<e8>`, `EMERGENCY:CLEAR:TXN:<e8>:<txn16>`)
are unauthenticated. An S3 that alters them only makes the Director sign something the
P4 rejects (K4): denial of service, never a clear.

## 4. Clearance transaction (shared policy `protocol/showduino_clear_txn.h`)

```
Director touch E-CLEAR ──DCLR:REQ──► S3 ──► P4  verify tag/episode/counter (persist)
                                              gate: GPIO25 released, no station blocker,
                                                    durable episode
                                              open transaction: fresh nonce
Director ◄── EMERGENCY:CLEAR:TXN + CLEAR_REQUEST ─┘   (dialog opens)
Director touch Confirm ──DCLR:CFM(txn)──► S3 ──► P4  verify tag/episode/counter/txn
                                              begin VERIFY: challenge all stations NOW
stations ◄── CHAL (broadcast, re-sent every 250 ms) ──┘
stations ── AUTH (RELEASED/PRESSED) ──► S3 ──► P4  only answers to challenges issued
                                              after the confirm count
P4 loop: every provisioned station RELEASED + full 001J decision OK → reserve GCL
         counters → re-check GPIO25 → store CLEAR → release latch → GCL → CLEAR_OK
         any ASSERT / fresh PRESSED / new trigger / storage fault → REJECTED:<reason>[:<id>]
         not all proven within 1000 ms → REJECTED:STATION_UNVERIFIED:<id>
```

The USB maintenance clear (`EMERGENCY:CLEAR` on the P4 console) runs the same
transaction with the local authority, so it also needs fresh station proofs.

## 5. J-05 closure evidence

| Entry point (§1) | After 001K | Evidence |
|---|---|---|
| E1–E3 Director touch / OS2 | signed REQ/CFM only | `handleUiCommand()`, `onEmergencyClearConfirm()`, `CT::EmergencyClear`; K1, K25 |
| E4 Director USB console | forwarded as plain text → refused `AUTH_REQUIRED`; `PROVISION` lines never forwarded | `handleUsbLine()` |
| E5 Compromised S3 | cannot forge, alter or replay a DCLR (no key, counters, episode, txn) | D2, D3, K2–K9, K23 |
| E6 S3 web UI | flag removed; plain text refused anyway | `CommsWebServerBase.h` |
| E7 P4 web API | flag removed; plain text refused anyway | `WebApiHandlerBase.h` |
| E8 Show cues | plain text → refused `AUTH_REQUIRED` | `showduino_clear_entry()`, P0 |
| E9 P4 USB console | maintenance authority (physical access), same verification; DCLR / CONFIRM refused on USB | P2, P3, K10, K21 |

Acceptance criterion: a compromised S3 cannot forge, modify or replay the operator
authorisation required to clear the P4 latch. **Met in the shared policy and host tests;
firmware compiled; not hardware tested.**

## 6. J-14 findings and residual limitations

What 001K changes:
* Station proofs now come **after** the authenticated confirm (verify sequence), never
  from before it (K14, mutant K14).
* Any ASSERT, any fresh PRESSED answer, any new trigger (incl. GPIO25) or storage fault
  ends the attempt (K15, K16b, K18, K19).
* Two station-side defects found and fixed while testing this:
  1. A re-press of a station that was still latched waited for the next 2 s heartbeat
     before asserting. It now asserts at once (K16b0, mutant K18).
  2. A latched, ACKed, released station repeated ASSERT every 2 s, which would have
     cancelled every Director clear request (K25). It now stops once ACKed and
     released (mutant K20).

**Can the single wireless path guarantee that no station is pressed at the instant of
clear? No.** Every station message crosses the Comms S3. An S3 that suppresses a
station's ASSERT and its later answers makes "pressed" indistinguishable from "silent
after a valid RELEASED". The boundary test K16c shows a clear accepted while a station
is physically pressed under exactly that attack. The exposure is limited to a press made
after the station's post-confirm RELEASED answer and before the clear is applied (at most
the 1000 ms verification window), and only with an actively hostile S3; after the clear
the station keeps re-asserting, which a hostile S3 can also hide. With an honest relay
the gap is the radio latency (tens of ms) and the station's immediate re-assert
re-latches the P4.

Options for a stronger guarantee (not implemented):

| Option | Guarantee | Cost |
|---|---|---|
| Hard-wired permissive loop: each station's second NC contact in series into a P4 input that must read closed to clear | No hidden press at the instant of clear, independent of radio | Wiring to every station; fits "readily available parts" |
| Independent receiver: a second ESP-NOW receiver wired to the P4 (not via the S3) that must also confirm | Defeats a single compromised S3 | One more cheap board; still radio |
| Operational policy: physical walk-round of every station before clear | Procedural only | Staff time |

Until one of these exists, J-14 remains an **open, disclosed residual** for any
installation where the Comms S3 could be compromised.

## 7. Credential provisioning and recovery

Enrol (Director released from any emergency, both boards on USB):
1. Director USB console: `PROVISION DIRECTOR NEW DIR-01` → prints
   `EMERGENCY:DIRKEY:SET:DIR-01:<64 hex>` and `fp=xxxxxxxx` once (never forwarded or
   shown on screen).
2. P4 USB console: paste the line → `DIRKEY:SET DIR-01 OK fp=xxxxxxxx`. Fingerprints
   must match. Clear the terminal scroll-back.
3. `EMERGENCY:DIRKEY:STATUS` on the P4; `PROVISION DIRECTOR STATUS` on the Director.

Replace a Director: enrol the new board as above (same or new ID). The old key stops
working at once (K12b). **Enrolled while an emergency is latched → restricted**: it cannot
clear that emergency (K12a). The authorised maintenance procedure is the P4 USB clear
(physical access, logged, still verifies every station); after any accepted clear the
restriction is lifted (K12c/d).

Revoke: `EMERGENCY:DIRKEY:REVOKE` (P4), `PROVISION DIRECTOR CLEAR` (Director).

Failures:
* Director lost or broken during an emergency → P4 USB clear (K10).
* Comms S3 dead → no station proofs → even the USB clear is refused while provisioned
  stations cannot answer; inspect each station, revoke and dismiss per 001J, then USB
  clear (K11).
* Director counter store lost (re-flash) → its messages look like replays; re-enrol
  with a new key (D3f).
* P4 Director record unreadable → nothing accepted; `EMERGENCY:DIRKEY:RESET`, re-enrol.
* No universal pairing password; no automatic pairing grants clear authority.

Flash encryption is not enabled on any board. Physical access to the Director or the P4
exposes the Director key; revoke and re-enrol after loss.

## 8. Threat model (operator authority)

| Threat | Result |
|---|---|
| Compromised S3 sends plain clear text | Refused `AUTH_REQUIRED` |
| Compromised S3 forges / alters a DCLR | Tag fails |
| Compromised S3 replays a captured DCLR (same episode, later episode, after either reboot) | Counter / episode / transaction reject |
| Compromised S3 alters P4→Director notices | Director signs wrong data → rejected (denial of service only) |
| Compromised S3 drops or delays DCLR or station traffic | Clear does not happen (fail closed) |
| Compromised S3 hides a press after a valid post-confirm proof | **Not prevented** (J-14 residual, §6) |
| Stolen Director | Holder can request and confirm clears like the operator; station proofs still required. Revoke on the P4. |
| Stolen P4 / Director flash dump | Keys readable (no flash encryption) |
| New Director enrolled mid-emergency | Restricted until a maintenance clear |
| Director reboot / disconnect | Nothing released; open request expires (K24) |

## 9. Local emergency authority (unchanged)

Activation never waits for authentication: GPIO25, USB, Director STOP and station ASSERT
still run the 001F engage sequence first. `triggerEmergency()` only additionally cancels
a pending clear transaction. Persisted latch, blocker set, emergency pixels, local
outputs, audio and reboot recovery are unchanged; the 001E–001J suites still pass. No
clear decision depends on S3 state: the S3 only relays signed or challenge-bound data.

## 10. Test evidence (host, policy only)

| Suite | Result |
|---|---|
| `tools/emergency-director-tests/test_director_auth.cpp` (new) | 51 pass, 0 fail |
| `tools/emergency-station-integration-tests/test_station_integration.cpp` (K1–K25 added) | 149 pass, 0 fail |
| `tools/emergency-auth-tests/test_station_auth.cpp` | 56 pass, 0 fail |
| Emergency Node protocol suite | 104 pass, 0 fail |
| Latch / button / Director sheet | 114 / 43 / 12 pass, 0 fail |
| Mutation check `tools/emergency-auth-tests/run_mutations.py` | 42 mutants (22 from 001J, 20 new), 42 killed |

All with `-Wall -Wextra -Werror` except the Director sheet suite (pre-existing warning).
Every suite and the mutation check run in CI on every push.

The integration harness uses the real station state machine, responder, P4 model,
clear decision, transaction, Director signer and Director acceptance policy, with real
HMAC. Simulated: radio, the S3 relay (which tests can make drop, hold, replay or forge
traffic), NVS, time. It does not run the Arduino wrappers.

Boundary tests (the guarantee does **not** hold): K16c (hostile S3 hides a press made
after a post-confirm RELEASED proof: clear accepted), K16d (that exposure is bounded by
the 1000 ms window).

## 11. Firmware build results (CI run 39029079990, commit cf8e43b)

| Target | Errors | Warnings | Flash | RAM | vs 001J |
|---|---|---|---|---|---|
| P4 Stage Engine | 0 | 1 (ESP32 core, pre-existing) | 1,210,260 B (92% of 1,310,720) | 84,700 B (25%) | +8,800 B flash, +384 B RAM |
| Director (S3) | 0 | 55, all seen in pre-existing code (`ShowduinoOsUi.h` enum deprecation, LVGL redefinitions); see note | 1,578,374 B (50% of 3,145,728) | 139,284 B (42%) | first CI build |
| C3 Emergency Node | 0 | 0 | 1,133,049 B (86%) | 39,840 B (12%) | +40 B |
| S3 Comms | 0 | 2 (`neopixelWrite` deprecated, pre-existing) | 1,396,087 B (41%) | 58,644 B (17%) | identical binary size |

* **P4 headroom is now about 100 KB (7.7%)** of the 1.25 MB app partition. No partition
  change was made (OTA and deployment implications not validated). The next large
  feature needs a planned partition change.
* **Director CI assumptions:** LVGL 9.2.2, current "GFX Library for Arduino", TAMC_GT911,
  Adafruit NeoPixel, `PartitionScheme=app3M_fat9M_16MB`. The repository does not record
  the library versions or partition actually used on the bench; the reported size depends
  on this assumption.
* **Director baseline (b3b3f95) does not build with LVGL 9.2.2**: its `lv_conf.h`
  included `<stdint.h>` in LVGL's assembly pass. Commit cf8e43b guards it (no effect on
  C/C++). There is therefore no like-for-like pre-001K Director size.
* Director warning attribution: GitHub keeps only ~10 annotations per level per step, so
  not all 55 warnings are visible; a CI notice now counts warnings in files changed by
  001J/001K (see the final run in the PR).

## 12. Hardware acceptance checklist (required before live use)

| # | Test | Pass criterion |
|---|---|---|
| D-01 | Enrol Director (§7) | Fingerprints match on both consoles; `DIRKEY:STATUS` shows enrolled, not restricted. |
| D-02 | Press and release a station; E-CLEAR, wait ≥ 5 s, Confirm | `CLEAR:VERIFYING` then `CLEAR_OK`; station returns to NORMAL. |
| D-03 | Same, but keep the station pressed | Refused `STATION_PRESSED:<id>`; Director names the station. |
| D-04 | Power off a provisioned station; E-CLEAR, Confirm | Refused `STATION_UNVERIFIED:<id>` within ~1 s. |
| D-05 | From the S3 USB/UART inject `EMERGENCY:CLEAR`, `EMERGENCY:CLEAR_CONFIRM` | Refused `AUTH_REQUIRED`; latch kept. |
| D-06 | Capture a DCLR pair with a sniffer, replay it (same and next emergency, after each reboot) | Refused (`REPLAY` / `WRONG_EPISODE` / `STALE_TRANSACTION`). |
| D-07 | Unenrolled Director presses E-CLEAR | "not enrolled" on screen; nothing sent. |
| D-08 | Enrol a new Director while latched | `RESTRICTED`; its clear refused; USB clear works and lifts it. |
| D-09 | Reboot the Director between E-CLEAR and Confirm | Nothing released; request expires. |
| D-10 | Pull the S3 during verification | Refused within ~1 s; latch kept. |
| D-11 | Press GPIO25 during verification | Refused / superseded; outputs unchanged. |
| D-12 | Re-press a latched station while the clear dialog is open | Request cancelled within radio latency. |
| D-13 | Time from Confirm to `CLEAR_OK` with 8 stations over the real relay | Well inside 1000 ms; record the value. |
| D-14 | Director USB `PROVISION DIRECTOR NEW` | Key shown only on USB; not in the Director UI log, not sent to the Stage. |

## 13. Rollout

All four boards must run 001K firmware: P4, Director, Comms S3, **and every C3**. A 001J
C3 keeps its heartbeat ASSERT while latched and would cancel every Director clear (fail
closed). Then enrol the Director. Until a Director is enrolled, only the P4 USB console
can clear (with station proofs). The web-UI clear flag no longer exists.

## 14. Status

| Item | Implemented in firmware | Automated tests | Compiled in CI | Hardware tested |
|---|---|---|---|---|
| Signed Director REQ/CFM, P4 enforcement, no plain-text clear (J-05) | yes | yes (D*, K1–K13, K23, P*) | P4, Director | **no** |
| Clearance transaction with post-confirm proofs (J-14 reduction) | yes | yes (K14–K19, K25) | P4 | **no** |
| Station re-press / heartbeat changes | yes (C3) | yes (K16b0, K25, mutants K18, K20) | C3 | **no** |
| Director enrolment / restriction / revocation | yes | policy yes; NVS code no | P4, Director | **no** |
| Equal-key refusal between station and Director keys | yes (001L: one shared function) | yes (001L L2a–L2f, mutants L02–L04) | P4 | **no** |
| J-14 guarantee against a hostile S3 | **not achievable** on the single path | boundary tests K16c/d | — | — |

Remaining blockers: J-14 residual (§6), keys without flash encryption on P4, Director
and C3, C3 SoftAP default password (001J J-10), no hardware validation, no safety
certification.
