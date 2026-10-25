# MISSION 001J — Emergency station authentication and replay protection

Status: **implemented in firmware, host-tested, compiled in CI. Not hardware tested.
Not a certified safety function.** Showduino's emergency features are entertainment
show-control interlocks. They are not life-safety equipment and must not replace
a certified emergency-stop or fire/evacuation system.

Branch `fix/emergency-persistent-latch`, PR #33 (draft, do not merge).
Inventory written before any change: `docs/emergency-trust-boundary-001J.md`.

## 1. Architecture and trust boundaries

```
 C3 Emergency Node  --ESP-NOW-->  S3 Comms Controller  --UART-->  P4 Stage Engine
 (holds its key)                  (untrusted relay, no keys)      (holds every key,
                                                                    decides clear)
```

* **C3 Emergency Node** (ESP32-C3): button, local latch, one 32-byte key.
* **S3 Comms Controller** (ESP32-S3): forwards frames. It is treated as possibly
  compromised. It holds no key and nothing it originates can change clear eligibility.
* **P4 Stage Engine** (ESP32-P4): global latch, station table, key table, decision.

Authentication is end to end between each C3 and the P4. ESP-NOW link encryption is
not used and would not help: ESP-NOW encryption terminates at the S3, which is the
party this design does not trust. Application-level HMAC is what protects the P4.

## 2. Protocol (wire formats)

All lines fit the ESP-NOW command field (95 characters) and the S3–P4 UART line
(180 characters), so nothing is truncated.

| Line | Direction | Authenticated | Purpose |
|---|---|---|---|
| `ESTOP:ASSERT:<id>:<name>` | C3 → P4 | **No** | Immediate latch. Never sets ONLINE or RELEASED. |
| `ANNOUNCE:...`, `STATUS:...` | C3 → P4 | No | Display only (name, firmware). No state change. |
| `CHAL:<episode8>:<nonce16>` | P4 → all (broadcast) | No (nonce is public) | Freshness challenge. |
| `AUTH:<id>:<episode8>:<nonce16>:IN=<0/1>:L=<0/1>:A=<0/1>:<tag32>` | C3 → P4 | **HMAC** | The only message that can set PRESSED/RELEASED, ONLINE or "verified". |
| `CMD:<id>:<counter8>:ACK|GCL:<episode8>:<tag32>` | P4 → C3 (unicast + broadcast copy) | **HMAC** | ACK teaches the episode; GCL returns a latched station to NORMAL after a P4 clear. |

* Tag: HMAC-SHA256 with the station key, truncated to 128 bits (RFC 2104 allows
  truncation to at least half the output; 128 bits is ample for online forgery).
  Canonical strings: `AUTH|v1|id|episode|nonce|in|latched|acked` and
  `CMD|v1|id|counter|op|episode`. Comparison is constant-time.
* HMAC comes from mbedtls on the ESP32 (`protocol/showduino_station_auth_esp32.h`)
  and from OpenSSL in the host tests. RFC 4231 vectors check the wrapper.

### 2.1 Replay protection

| Message | Mechanism | Across P4 restart | Across C3 restart |
|---|---|---|---|
| AUTH | P4 per-boot challenge table: nonce must be outstanding, ≤ 1500 ms old, for the current episode, and is single-use per station. | Table is RAM: every pre-restart nonce is unknown → rejected. | Station keeps no AUTH state; it answers fresh challenges. |
| CMD | Station accepts only `counter > last accepted`; the new counter is written to NVS **before** the command is applied. | P4 counter resumes above a persisted block limit (persist-before-use, block of 256). | Station reloads its last counter from NVS. |
| GCL | Also bound to the episode the station learned from an authenticated ACK. A fresh local latch forgets the episode. | Episodes come from the same persisted counter, so ids never repeat. | Episode is RAM: a rebooted station must learn it again from a new ACK. |

### 2.2 Clear policy (P4)

`showduino_station_clear_decision()` (shared by firmware and tests):

1. Storage: blocker record, latch record, key record and counter must be durable
   and readable; while latched with provisioned stations, a durable episode must exist.
2. Station table must not have overflowed this episode.
3. No known station may be PRESSED, unverified or OFFLINE (MISSION 001H/001I rules).
4. At CONFIRM only: **every provisioned station** must have a verified AUTH reporting
   RELEASED that answered one of the two most recent challenges **and** a challenge
   issued after the CLEAR_REQUEST (`showduino_station_proof_since`). Challenges run
   every 500 ms while latched, so an accepted proof is at most about one second old.
5. One GCL counter per provisioned station is reserved (persisted) before the clear
   is applied; if that fails, the clear is refused.

A provisioned station that is silent, switched off, out of range or dropped by the
relay therefore blocks clear. Remove a decommissioned station with
`EMERGENCY:KEY:REVOKE` (§4.3).

### 2.3 Unauthenticated ASSERT

ASSERT stays unauthenticated on purpose: authentication must never delay a local
emergency. An ASSERT always latches the P4, even when the station table is full. It
records that the ID asserted in this episode (so it stays in the persisted blocker
set), but it never sets ONLINE, RELEASED or "verified", so it can never satisfy
clearance. Consequence: a forged ASSERT can cause a nuisance emergency (it always
could). A forged ASSERT for an ID that is not provisioned creates a blocker that only
an authorised local action can remove (`EMERGENCY:STATION:DISMISS`).

## 3. Threat model

| Threat | Result with 001J | Notes |
|---|---|---|
| Forged RELEASE / ANNOUNCE / STATUS from any radio or the S3 | Rejected. ANNOUNCE/STATUS are display only; AUTH needs the key. | Host tests S2, S3. |
| Replay of a captured AUTH (same episode, later, after P4 reboot) | Rejected: nonce single-use, 1.5 s window, per-boot table. | S4, S5, S8. |
| Replay of a captured CMD (ACK, GCL) incl. after C3 reboot | Rejected by the persisted counter; GCL also by episode. | S5d, S6, S17, S17f. |
| S3 compromised: forge station state | Not possible without keys. | |
| S3 compromised: originate ACK / GCL / EMERGENCY:CLEAR to stations | Stations reject all unauthenticated variants; S3 code no longer sends them. | S16. |
| S3 compromised: drop, delay, reorder or flood frames | **Not prevented.** Dropped AUTH → proof missing → clear refused (fail closed). Dropped ASSERT → that station's press is not seen until its next AUTH, which shows PRESSED. Dropping both hides a press entirely: availability limit. Delay is bounded by the 1.5 s window. | S13d, S15. End-to-end authentication protects integrity, not delivery. |
| S3 compromised: drive Director CLEAR_REQUEST/CONFIRM over UART | **Open blocker (J-05).** The P4 still treats UART commands as Director commands. Station proofs still apply, so the S3 cannot clear while any provisioned station is pressed or silent, but it can clear an emergency raised from the Director, USB or a closed hard-wired loop. Needs an authenticated operator channel. | |
| Stolen station (physical access to one C3) | Its key can be read from flash (no flash encryption). The attacker can impersonate **that station only**. Revoke it on the P4 and re-key the replacement. | J-08. |
| Stolen / tampered P4 | All keys readable. Re-key every station after recovering a P4. | J-08. |
| Lost station during an emergency | Blocks clear (fail closed). Recovery: replacement with a new key (§4.2) or revoke + dismiss (§4.3). | S10, S26. |
| Power failure of P4 | Latch, blocker set, keys and counter limit are in NVS; episode is re-allocated; stations re-learn it from ACK. | S14, S20. |
| Power failure of C3 | Key and last counter in NVS; local latch restarts from the button. | S6. |
| Storage failure (P4 counter, key record, blocker set; C3 counter) | Clear refused; station keeps asserting. Never a silent downgrade. | S11. |
| Legacy (pre-001J) station | Its ASSERT still latches; its ANNOUNCE cannot release it; it blocks clear once it has asserted. No insecure fallback. | S12. |
| Station table filled with forged IDs | Real ASSERTs still latch; clear refused until local maintenance. | S18. |
| Predictable P4 nonces | P4 nonces are HMAC(boot seed, counter); the seed comes from the SAR ADC entropy source at boot. | Hardware check H-12. |
| SoftAP web on the C3 (shared password `showduino`) | Limited to PIXEL commands; cannot ACK, clear, rename a provisioned station or change its key. | Password is still a published default (J-10). |

## 4. Provisioning, replacement, revocation

Keys are entered only over the **local USB console** of each board. The P4 refuses
key commands from the Comms UART and the web API, and handles them before the
command log line, so key text is never echoed to logs.

### 4.1 Register a new station

1. Station released, not latched. Connect the C3 by USB (115200 baud).
2. On the C3: `PROVISION NEW ESTOP-07` → it sets its ID, stores a new random key
   (true RNG: Wi-Fi is running) and prints, once:
   `EMERGENCY:KEY:SET:ESTOP-07:<64 hex>` and a fingerprint `fp=xxxxxxxx`.
3. On the P4 USB console paste that line. The P4 prints `KEY:SET ESTOP-07 OK fp=xxxxxxxx`.
   The two fingerprints must match.
4. `EMERGENCY:AUTH:STATUS` on the P4 should show the station `verified=1 online=1`
   within a few seconds.
5. Clear the terminal scroll-back; do not store the key elsewhere.

Alternative: generate a key on a laptop (`openssl rand -hex 32`) and enter it with
`PROVISION SET ESTOP-07 <hex>` on the C3 and `EMERGENCY:KEY:SET:...` on the P4.

### 4.2 Replace a broken station (same ID)

Do steps 4.1.1–4.1.3 on the replacement with the **same ID**. `KEY:SET` replaces the
old key; the old board can no longer authenticate. This is allowed while the P4 is
latched: the station keeps blocking until the replacement proves RELEASED with the
new key (no silent recovery). Never reuse a retired key.

### 4.3 Retire a station or recover from a lost one

* P4: `EMERGENCY:KEY:REVOKE:ESTOP-07`. The station no longer has to prove anything,
  but if it asserted in this episode it keeps blocking (it is now unverifiable).
* P4: `EMERGENCY:STATION:DISMISS:ESTOP-07` removes the entry. Only allowed for an
  ID with no key. **Inspect the physical station before dismissing.** Logged.
* C3 (if available): `PROVISION CLEAR`.

### 4.4 Factory reset / corrupt records

* C3 with an unreadable key record: it cannot authenticate (blocks clear). Run
  `PROVISION NEW` or `PROVISION SET` again and re-enter the key on the P4.
* P4 with an unreadable key record: clear is refused. `EMERGENCY:KEY:RESET` (only
  accepted when the record is unreadable) wipes it; re-enter every station key.
* P4 counter record unreadable: clear refused, station commands held. MISSION 001L adds
  `EMERGENCY:RECOVER:COUNTER:COUNTER-LOST` (USB only); see
  `docs/emergency-maintenance-recovery-001L.md` §3.5.

## 5. Compatibility and rollout

* Pre-001J C3 firmware cannot answer challenges. After the P4 update such a station
  still latches the P4, but once it has asserted it blocks clear until it is updated
  and provisioned, or dismissed locally. It is never trusted to authorise a clear.
* Pre-001J S3 firmware still fans out unauthenticated ACK / GLOBAL:OBSERVED /
  EMERGENCY:CLEAR; 001J stations ignore them, but update the S3 as well.
* A 001J C3 talking to a pre-001J P4 never receives a valid CMD, so it will not
  return to NORMAL after a P4 clear (fail safe, not usable).

Order: update S3 → update P4 → update and provision each C3 → bench-test (§7).
Do this with the show stopped. Expect the P4 to refuse clears until every
registered station has been provisioned.

## 6. Test evidence (host, policy only)

| Suite | Checks | Result |
|---|---|---|
| `tools/emergency-auth-tests/test_station_auth.cpp` | 56 | pass |
| `tools/emergency-station-integration-tests/test_station_integration.cpp` | 103 | pass |
| `tools/emergency-auth-tests/run_mutations.py` | 22 mutants | 22 killed |
| latch / button / director sheet / emergency node suites | unchanged | pass |

All built with `-Wall -Wextra -Werror` (the Director sheet suite excepted, as before).
CI run 39021270035 (commit d072eed): host tests, P4, C3 and S3 compiles all succeed.
P4 0 errors / 1 pre-existing core warning, 1,201,460 B flash (91%), 84,316 B RAM;
C3 0 / 0, 1,133,009 B (86%), 39,840 B; S3 0 / 2 pre-existing, 1,396,087 B (41%),
58,644 B. Details in `docs/firmware-p4-emergency-latch.md`.
The integration harness uses the real C3 state machine, the real responder, the real
P4 model and clear decision, and real HMAC; the radio, relay, NVS and time are
simulated. It does not run the Arduino wrappers.

## 7. Hardware acceptance checklist (required before any live use)

| # | Test | Pass criterion |
|---|---|---|
| H-01 | Provision two stations (§4.1) | Fingerprints match; `AUTH:STATUS` shows both verified/online. |
| H-02 | Press station A | P4 latches within the 001F timing; station shows ACKed (authenticated ACK). |
| H-03 | Release A, request + confirm clear | Accepted only after A proves RELEASED; A returns to NORMAL via GCL. |
| H-04 | Power off station B, try to clear | Refused (`STATION_UNVERIFIED`). |
| H-05 | Unprovisioned C3 (or legacy firmware) presses | Latches; clear refused until dismissed or provisioned. |
| H-06 | Replay: capture AUTH/CMD frames with a sniffer C3 and re-inject | No state change; no clear. |
| H-07 | Reboot P4 while latched | Blockers restored; stations re-learn the episode; clear needs fresh proofs. |
| H-08 | Reboot C3 while latched | Counter survives; old CMDs rejected. |
| H-09 | Replace a station during an emergency (§4.2) | Blocks until replacement proves RELEASED. |
| H-10 | Pull S3 UART TX/RX during an emergency | Clear refused (no proofs); latch kept. |
| H-11 | Flood ESP-NOW with forged ANNOUNCE/ASSERT | Real ASSERTs still latch; no release. |
| H-12 | P4 boot: `EMERGENCY:AUTH:STATUS` shows `nonce=seeded`; inspect that nonces differ across reboots | Seed works with the SAR ADC source on this board. |
| H-13 | NVS wear: 1 h latched with 8 stations | No storage faults; check write counts. |
| H-14 | C3 SoftAP: try `/api/pixel` with `ESTOP:ACK:LATCHED`, `/api/identity` ID change | Rejected. |
| H-15 | Timing: challenge/answer round trip over the real S3 relay | Well under the 1500 ms window under show load. |

## 8. Known limitations and open items

* **J-05:** closed in MISSION 001K (signed Director clear; `docs/emergency-director-auth-001K.md`).
* **J-08:** keys stored without flash encryption on both boards.
* **J-10:** C3 SoftAP uses a published default password.
* **J-14:** reduced in 001K (proofs collected after the confirm); a hostile S3 can still
  hide a press made after a post-confirm proof. See 001K §6.
* **Availability:** a compromised S3 can always hide traffic. Fail-closed behaviour
  only guarantees that hidden traffic cannot *release* anything.
* Optional policy not implemented: treat a provisioned station going offline while
  idle as an emergency (supervision). Decide per installation.
* No hardware validation has been performed. No safety certification is claimed.
