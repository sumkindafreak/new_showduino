# MISSION 001J — trust boundaries and message authority (inventory, pre-change)

Status: inspection record for branch `fix/emergency-persistent-latch`, starting commit `c7261c1`.
Draft PR #33. Not safety-certified. Not hardware-validated.

## 1. Physical path (as implemented before 001J)

```
Emergency Node (ESP32-C3, ESTOP-xx)
   |  ESP-NOW, 116-byte ShowduinoNodePacket, encrypt=false, unsigned
   v
Comms Controller (ESP32-S3)  -- untrusted transport relay
   |  UART, lines <= SHOWDUINO_COMMS_LINE_MAX (180 bytes)
   |  inbound  NODE:EMERGENCY:<cmd>   (ASSERT sent as priority)
   |  outbound ROUTE:EMERGENCY:<id>:<seq>:<cmd>
   v
Stage Engine (ESP32-P4)  -- decision point: global latch, clear policy, NVS
```

ESP-NOW packet: `nodeType[16]`, `command[96]`, `sequence (u32)`, fixed 116 bytes. The
C3 discriminates packets by size. There is no room for a tag in the existing packet.
The UART line limit is 180 bytes, so a compact authenticated payload fits without truncation.

## 2. Trust boundaries

| Component | Holds keys? | Trusted for | Not trusted for |
|---|---|---|---|
| Physical button on a station | n/a | the station's own input state (through its own firmware) | — |
| Emergency Node C3 firmware | (001J) its own station key | its own state | anything it cannot prove |
| ESP-NOW radio | no | delivery only | any authority: any radio can transmit |
| S3 Comms firmware | **no** (001J design) | relaying bytes | any authority decision; any clear; any identity |
| UART S3 <-> P4 | n/a | nothing (treated as untrusted) | authority |
| P4 Stage Engine | all station keys (001J) | global latch, clear policy, persistence | — |
| Director / Comms command source | n/a | operator intent (see §6, blocker) | not cryptographically bound (blocker) |

## 3. Message authority inventory (before 001J)

| # | Message (direction) | Path | Authority it claims | Current check | Effect today |
|---|---|---|---|---|---|
| M1 | `ESTOP:ASSERT:<id>:<name>` (C3→P4) | ESP-NOW → S3 → UART | physical press | none (bare) | global latch; station allocated; blocks clear until verified |
| M2 | `ANNOUNCE:<mac>:...:ID=:IN=:L=:A=` (C3→P4) | ESP-NOW → S3 → UART | physical release | identity pin by ID/MAC only | **sets RELEASED and online** — forged release possible |
| M3 | `STATUS:...` (C3→P4) | ESP-NOW → S3 → UART | liveness, L=1 latch request | single online station | can request global latch |
| M4 | `ESTOP:ACK:LATCHED` (P4→C3) | UART → S3 → ESP-NOW | P4 acknowledgement | none | sets C3 acked |
| M5 | `ESTOP:GLOBAL:OBSERVED` (P4→C3) | UART → S3 → ESP-NOW | P4 global clear observed | none | **C3 local latch reset if released** |
| M6 | `EMERGENCY:CLEAR` fan-out (S3→C3) | S3 originated | P4 cleared | none; S3 decides from its own UART lines | maps to M5 on C3 — **S3 can reset C3 latches** |
| M7 | `ESTOP:ACK:LATCHED` fan-out (S3→C3) | S3 originated | P4 latched | none | sets C3 acked |
| M8 | `ESTOP:NODE:ID:...` / `NAME:` (P4 or any radio→C3) | ESP-NOW | operator identity change | none | **any radio can rename a station** |
| M9 | `ESTOP:REARM` (any radio→C3) | ESP-NOW | local maintenance reset | none | C3 local reset (not P4) |
| M10 | `ESTOP:OWN:GRANT` (any radio→C3) | ESP-NOW | pixel/soft-AP ownership | none | not clear authority; pixel ownership |
| M11 | `EMERGENCY:CLEAR_REQUEST` / `CLEAR_CONFIRM` / `CLEAR` (S3 or Director→P4) | UART (source = Comms) | Director / operator clear | P4 clear policy (stations verified released) | **a compromised S3 can drive the Director clear sequence** |
| M12 | `STATE:EMERGENCY:*`, `STATE:SHOW:*`, `ELOCKED`, `ECLEARED` (P4→S3) | UART | P4 state for display and S3 fan-out to audio/lamp/pixel/mosfet | none | S3 derives its own emergency belief from these |
| M13 | `ASSERT` received by P4 from S3 (priority) | UART | any station | none | same as M1 (fail-safe direction) |

Confirmed evidence (file and function):
- M2: `EmergencyNodeLink.cpp` `emergencyNodeLinkHandleReport`, ANNOUNCE branch: `showduino_station_interpret_line` → `showduino_station_model_on_announce` sets `inputOpen`, `reportKnown`, `online`, from unauthenticated fields.
- M5/M6: `C3 EmergencyProtocol.cpp` `emergencyProtocolApply` → `showduino_emergency_parse_command`; `"EMERGENCY:CLEAR"` maps to `SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED`. `S3 ProtocolBridge.cpp` `fanoutEmergency()` sends `EMERGENCY:CLEAR` to emergency nodes on P4 clear.
- M8/M9: the same C3 apply function handles `ID:`, `NAME:` and `REARM` with no origin check.
- M11: `ShowduinoStageEngineP4.ino` line ~329 `handleCommand(String(cmd), CommandSource::Comms)` for UART commands; `handleEmergencyClearRequest`, `handleEmergencyClearConfirm`, `applyEmergencyClear`.
- Relay: `S3 EspNowTransport.cpp` `noteEmergencyPeer` learns station MAC from unauthenticated ANNOUNCE and ASSERT. `espNowTransportSendToEmergencyNode` unicasts by that MAC, so a poisoned mapping misroutes P4 commands (availability).

## 4. Current message sequences

Normal latch: station press → ASSERT burst → S3 priority UART → P4 `applyStationLatch` → `triggerEmergencyWireless` (outputs first, MISSION 001I) → ACK routed back via S3.

Clear: P4 accepts Director clear (requires all stations verified released) → `applyEmergencyClear` persists latch clear, then S3 sees `STATE:...ECLEARED` and fans out `EMERGENCY:CLEAR` to every station; each C3 maps that to GLOBAL_OBSERVED.

## 5. Findings that 001J must fix or disclose

| ID | Finding | Severity | Plan |
|---|---|---|---|
| J-01 | Unauthenticated ANNOUNCE can assert RELEASED and ONLINE (forged release) | Critical | AUTH-only verified state (§7) |
| J-02 | S3 can reset C3 local latches by fan-out `EMERGENCY:CLEAR` | High | remove S3 emergency fan-out; P4 authenticated GCL |
| J-03 | Any radio can rename a station (ID/NAME) or reset it (REARM) | High | reject over wireless; serial provisioning only |
| J-04 | Stations accept unauthenticated ACK / GLOBAL:OBSERVED | High | authenticated CMD with persistent counters |
| J-05 | Compromised S3 can drive the Director clear command sequence over UART | **Critical, blocker** | cannot be closed by station authentication alone; needs authenticated operator channel (§9) |
| J-06 | Unprovisioned/legacy stations would be able to satisfy clear | Critical | legacy assertions block clear (reportKnown=0) |
| J-07 | No replay protection across P4 or C3 restart | High | per-boot challenge nonces; persistent command counters |
| J-08 | Keys in flash are not encrypted (no flash encryption assumed) | Medium, accepted with disclosure | document; recommend eFuse flash encryption |

## 6. Blocker: Director/Comms clear authority (J-05)

The P4 accepts Director clear commands that arrive over the Comms UART. A compromised S3 can therefore send a clear request and confirmation without the operator. The station-level authentication in 001J ensures that clearance also requires every station to be verified released (a forged or replayed release cannot satisfy it). It does not prove operator intent. Closing this needs an authenticated operator channel from the Director to the P4. That is outside 001J and is a release blocker.

## 7. Design (001J)

**Primitive.** HMAC-SHA256 from mbedtls on the ESP32 firmware, and OpenSSL HMAC in host tests. Both are established implementations, and RFC 4231 vectors check the wrapper. Tags are truncated to 128 bits, which is the RFC 2104 minimum guidance. No custom cryptography.

**Keys.** One random 256-bit key per emergency station, generated on the P4 with the hardware RNG (`esp_random`) and provisioned locally. The P4 stores all station keys in NVS. The C3 stores its own key in NVS. The S3 holds no keys.

**Provisioning** (local USB only, refused while the P4 emergency is latched):
- P4: `EMERGENCY:PROVISION:<ID>` prints a new key once. `EMERGENCY:REVOKE:<ID>` deletes a key.
- C3: serial `PROVISION <ID> <HEX64>` and `PROVISION CLEAR`, refused while the station is latched or its button is pressed.
- A replacement board gets a new key. Its predecessor's key no longer verifies.

**Challenge-response for station state (AUTH).**
- P4 broadcasts `CHAL:<E>:<N>` every 1 s: `E` is the episode id (0 when idle), `N` is a fresh 64-bit nonce from the hardware RNG. It travels as `ROUTE:EMERGENCY:ALL:<seq>:CHAL:...`.
- A station replies `AUTH:<ID>:<E>:<N>:<IN>:<L>:<A>:<TAG>`, where `TAG = HMAC(K_ID, "AUTH|v1|ID|E|N|IN|L|A")`, truncated to 128 bits.
- The P4 accepts an AUTH only when: the ID has a key; the tag verifies (constant time); `N` is an outstanding challenge issued in this boot; it is not already used for this station; it arrived within 1500 ms; and `E` equals the current episode. The nonce is then consumed.
- Only accepted AUTH updates identity state: RELEASED/PRESSED, online, announced latch. ANNOUNCE and STATUS become display-only.
- Replay across P4 restart fails, because nonces from the previous boot are not outstanding. Replay across C3 restart fails for the same reason. Replay after a new episode fails, because `E` no longer matches.

**Commands P4 → station.** `CMD:<ID>:<C>:<OP>:<E>:<TAG>` with `TAG = HMAC(K_ID, "CMD|v1|ID|C|OP|E")`.
- `OP` is `ACK` or `GCL`. `GCL` means the P4 global episode `E` has been cleared.
- `C` is a monotonic counter. The P4 allocates it from a persisted block (reserve 256 at a time, so reboots never reuse values). The C3 persists the last accepted value and rejects any `C` not greater than it. The write happens before the command is applied. If the write fails, the command is refused.
- The C3 has no wireless clear. It clears its local latch only on an authenticated `GCL` whose `E` matches the episode it latched under.
- `ID:`, `NAME:` and `REARM` are removed from the wireless path. `EMERGENCY:CLEAR` from any radio is rejected.

**Forged, delayed and replayed messages.**
- ASSERT stays unauthenticated and keeps its immediate effect. It latches the global emergency and allocates a record for the ID. It does not set online, released or verified. A record without a verified AUTH blocks clear. An ASSERT for an unknown ID therefore cannot satisfy clearance, and it cannot create trusted state.
- A forged or late AUTH is rejected. A forged or replayed CMD is rejected. Failed authentication never changes the global latch.

**Legacy stations.** A station without a key cannot send a valid AUTH. Its assertions block clear until it is provisioned. Nothing in the legacy path can release it.

**Availability (not guaranteed).** A compromised S3 can drop, delay, duplicate, reorder or flood messages. It can stop challenges reaching stations, and it can stop AUTH reaching the P4. In each case the P4 fails closed: clear stays blocked. End-to-end authentication does not give delivery guarantees.

## 8. Residual limitations (001J)

- ESP-NOW packets are still unencrypted at link level. Application-level tags give integrity and origin for state-changing messages. They do not hide traffic.
- Keys are in plaintext NVS unless the device is flash-encrypted. No eFuse flash encryption is assumed.
- No authorised maintenance-recovery procedure exists in firmware. A station that is missing or unverifiable blocks clear until it is provisioned.
- Operator clear intent is not authenticated (J-05).
- No hardware measurement of challenge timing or of replay windows has been made.

## 9. Hardware acceptance tests still required

1. Provision two stations over USB. Confirm that a forged ANNOUNCE with RELEASED does not clear.
2. Power-cycle the P4 mid-episode. Confirm stored blockers hold and stale AUTH is rejected.
3. Power-cycle a C3 with a stored counter. Confirm an old GCL is rejected.
4. Replace a station board with a new key. Confirm the old key is rejected.
5. Measure challenge round-trip and the time to the first output change (scope).
6. Confirm that a PROVISION attempt while latched is refused on both sides.

## 10. Implementation status (after 001J commits b2b5d00, f787589 and later)

Sections 1–9 above are the pre-change inventory and are kept as written. Status:

| ID | Finding | Status |
|---|---|---|
| J-01 | Forged ANNOUNCE could release | **Fixed.** ANNOUNCE/STATUS display only; only a verified AUTH sets RELEASED/ONLINE. Tests S2, S3. |
| J-02 | S3 fan-out reset station latches | **Fixed.** S3 `fanoutEmergency()` sends nothing to stations; stations reject plain clear tokens. S16. |
| J-03 | Radio could rename / rearm a station | **Fixed.** ID/NAME/REARM local USB only; ID locked once provisioned; SoftAP limited to PIXEL. |
| J-04 | Unauthenticated ACK / GLOBAL:OBSERVED | **Fixed.** Authenticated CMD with persisted counters and episode binding. S5d, S6, S16, S17f. |
| J-05 | S3 can drive Director clear over UART | **Closed in MISSION 001K (host-tested, compiled; not hardware tested).** Only a Director-signed DCLR (or the P4 USB console) can clear; plain clear text from the UART is refused. See `docs/emergency-director-auth-001K.md` §5. |
| J-06 | Legacy stations could satisfy clear | **Fixed.** Legacy ASSERT blocks; legacy ANNOUNCE cannot release. S12. |
| J-07 | No replay protection across restarts | **Fixed.** Per-boot nonces, persisted counters, episode ids from a persisted counter. S4–S6, S8. |
| J-08 | Keys in plaintext flash | **Open, disclosed.** Recommend eFuse flash encryption + secure boot for deployments that need it. |

Found during implementation:

| ID | Finding | Status |
|---|---|---|
| J-09 | (pre-existing) A full station table made a real ASSERT return without latching | **Fixed.** ASSERT always latches; overflow blocks clear. S18, mutant M10. |
| J-10 | C3 SoftAP: published default password; `/api/pixel` passed any command to the station; `/api/identity` changed the ID | **Mitigated.** Web is PIXEL-only and cannot change a provisioned ID. Default password remains (open, Medium). |
| J-11 | S3 per-station MAC table learned from unauthenticated frames (downlink diversion) | **Mitigated.** CHAL broadcast; every CMD also sent as a broadcast copy. Remains an availability limit. |
| J-12 | ESP32-P4 has no radio: `esp_random()` is pseudo-random unless the SAR ADC entropy source is on | **Addressed in design.** Nonces = HMAC(boot seed, counter); seed drawn with `bootloader_random_enable()`. Needs hardware check H-12. |
| J-13 | Proof freshness by millisecond timestamp accepted a same-millisecond older challenge | **Fixed** before firmware: per-boot challenge sequence. Mutants M08, M12. |
| J-14 | ~1 s between last accepted proof and confirm in which a press hidden by a compromised S3 (ASSERT and AUTH dropped) is not seen | **Reduced in 001K, still open.** Proofs must now follow the authenticated confirm; any ASSERT or fresh PRESSED aborts. A hostile S3 can still hide a press made after a post-confirm proof (≤ 1000 ms window). Not solvable on the single wireless path; see 001K §6. |
| J-15 | Forged ASSERT with an invented ID becomes a permanent blocker | **By design (fail closed).** Local USB `EMERGENCY:STATION:DISMISS`. |

Design, protocol and provisioning: `docs/emergency-station-auth-001J.md`.
