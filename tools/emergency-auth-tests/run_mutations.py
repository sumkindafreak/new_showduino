#!/usr/bin/env python3
"""MISSION 001J/001K/001L mutation check for station authentication, Director operator
authentication, the clearance transaction and the clear entry-point policy.

Each mutant removes or weakens one check in a temporary copy of protocol/. Both
the auth suite and the station integration suite are rebuilt against the copy
(-Wall -Wextra -Werror). A mutant is KILLED when compilation succeeds and at
least one suite fails. A mutant that does not compile is reported as INVALID
(the script fails, so the mutant must be rewritten). A SURVIVING mutant fails
the script unless it is listed in EQUIVALENT with a reason.

Run from the repository root:  python3 tools/emergency-auth-tests/run_mutations.py
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SUITES = [
    "tools/emergency-auth-tests/test_station_auth.cpp",
    "tools/emergency-station-integration-tests/test_station_integration.cpp",
    "tools/emergency-director-tests/test_director_auth.cpp",
    "tools/emergency-node-tests/test_emergency_node.cpp",
    "tools/emergency-recovery-tests/test_recovery_001l.cpp",
]

# (id, file, old, new, occurrence index (0-based) or None for "must be unique")
MUTANTS = [
    ("M01 nonce single-use bit ignored", "showduino_station_auth.h",
     "    if (s->usedMask & bit) return 0;\n", "", None),
    ("M02 freshness window ignored", "showduino_station_auth.h",
     "    if (age > SHOWDUINO_AUTH_WINDOW_MS) return 0;\n", "    (void)age;\n", None),
    ("M03 challenge episode ignored", "showduino_station_auth.h",
     "    if (s->episode != episodeNow) return 0;\n", "    (void)episodeNow;\n", None),
    ("M04 AUTH tag not compared", "showduino_station_auth.h",
     "  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);",
     "  (void)expect; return 1;", 0),
    ("M05 CMD tag not compared", "showduino_station_auth.h",
     "  return showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);",
     "  (void)expect; return 1;", 1),
    ("M06 counter freshness >=", "showduino_station_auth.h",
     "  return counter > lastAccepted;", "  return counter >= lastAccepted;", None),
    ("M07 counter block persisted after use", "showduino_station_auth.h",
     "    if (!persist(ctx, newLimit)) return 0;", "    (void)persist(ctx, newLimit);", None),
    ("M08 challenge reset rewinds sequence", "showduino_station_auth.h",
     "  t->lastSeq = keep;", "  (void)keep; t->lastSeq = 0;", None),
    ("M09 ct_equal checks first byte only", "showduino_station_auth.h",
     "  for (size_t i = 0; i < n; ++i) acc |= (uint8_t)(a[i] ^ b[i]);",
     "  for (size_t i = 0; i < n && i < 1; ++i) acc |= (uint8_t)(a[i] ^ b[i]);", None),
    ("M10 ASSERT does not latch with a full table", "showduino_station_model.h",
     "    res->globalLatch = 1;\n    int slot = showduino_station_model_find_id(t, n, id);",
     "    int slot = showduino_station_model_find_id(t, n, id);", None),
    ("M11 offline station does not block", "showduino_station_model.h",
     "  return (m->inputOpen || !m->reportKnown || !m->online) ? 1 : 0;",
     "  return (m->inputOpen || !m->reportKnown) ? 1 : 0;", None),
    ("M12 proof age not checked", "showduino_station_model.h",
     "        m->proofChalSeq == 0 || (int32_t)(m->proofChalSeq - sinceSeq) < 0) {",
     "        m->proofChalSeq == 0) { (void)sinceSeq;", None),
    ("M13 missing provisioned station not counted", "showduino_station_model.h",
     "    if (slot < 0) { missing++; continue; }", "    if (slot < 0) { continue; }", None),
    ("M14 table overflow ignored", "showduino_station_model.h",
     "  if (tableOverflow) return SHOWDUINO_CLEAR_BLOCK_OVERFLOW;", "  (void)tableOverflow;", None),
    ("M15 proof window ignores the request", "showduino_station_model.h",
     "  return (int32_t)(requestSeq - recent) > 0 ? requestSeq : recent;",
     "  return recent;", None),
    ("M16 unverified AUTH accepted (tag result ignored)", "showduino_station_model.h",
     "    if (!tagOk) return 1;", "    (void)tagOk;", None),
    ("M17 dismiss allowed for provisioned IDs", "showduino_station_model.h",
     "  if (idIsProvisioned) return 0;", "  (void)idIsProvisioned;", None),
    ("M18 station GCL episode not checked", "showduino_station_responder.h",
     "  } else if (!showduino_auth_gcl_matches_episode(st->latchedEpisode, m.episode)) {",
     "  } else if (0) {", None),
    ("M19 station applies before persisting", "showduino_station_responder.h",
     "  if (!persist(ctx, m.counter)) return SHOWDUINO_RESP_REJECT_PERSIST;",
     "  (void)persist(ctx, m.counter);", None),
    ("M20 station ignores CMD identity", "showduino_station_responder.h",
     "  if (!showduino_emergency_id_equal(m.id, id)) return SHOWDUINO_RESP_REJECT_IDENTITY;",
     "", None),
    ("M21 fresh latch keeps the old episode", "showduino_station_responder.h",
     "  if (l != st->prevLatched) st->latchedEpisode = 0;", "", None),
    ("M22 station accepts commands while unlatched", "showduino_station_responder.h",
     "  if (!latched) return SHOWDUINO_RESP_REJECT_STATE;", "  (void)latched;", None),
]

# MISSION 001K: Director authority, clearance transaction, entry policy.
MUTANTS += [
    ("K01 Director tag not compared", "showduino_director_auth.h",
     "  const int ok = showduino_auth_ct_equal(expect, m->tag, SHOWDUINO_AUTH_TAG_BYTES);",
     "  const int ok = 1;", None),
    ("K02 Director episode not checked", "showduino_director_auth.h",
     "  if (episodeNow == 0 || m.episode != episodeNow) return SHOWDUINO_DIR_REJECT_EPISODE;",
     "  (void)episodeNow;", None),
    ("K03 restricted Director allowed", "showduino_director_auth.h",
     "  if (rec->restricted) return SHOWDUINO_DIR_REJECT_RESTRICTED;", "", None),
    ("K04 CFM transaction not checked", "showduino_director_auth.h",
     "  if (m.op == SHOWDUINO_DIR_OP_CFM) {\n    if (!openTxnHex", "  if (0) {\n    if (!openTxnHex", None),
    ("K05 Director counter persisted after accept", "showduino_director_auth.h",
     "  if (!persist || !persist(ctx, m.counter)) return SHOWDUINO_DIR_REJECT_PERSIST;",
     "  if (persist) (void)persist(ctx, m.counter);", None),
    ("K06 Director identity not checked", "showduino_director_auth.h",
     "  if (strcmp(rec->id, m.id) != 0) return SHOWDUINO_DIR_REJECT_IDENTITY;", "", None),
    ("K07 Director counter freshness not checked", "showduino_director_auth.h",
     "  if (!showduino_cmd_counter_is_fresh(rec->lastCounter, m.counter)) {\n    return SHOWDUINO_DIR_REJECT_REPLAY;\n  }",
     "", None),
    ("K08 Director signer reuses a transaction", "showduino_director_auth.h",
     "  s->txnHex[0] = '\\0';\n  return ok;", "  return ok;", None),
    ("K09 request window not enforced at confirm", "showduino_clear_txn.h",
     "  if ((uint32_t)(nowMs - t->openedMs) > SHOWDUINO_CLEAR_TXN_OPEN_MS) {\n    showduino_clear_txn_abort(t);\n    return SHOWDUINO_TXN_REJECT_EXPIRED;\n  }\n  if (episodeNow == 0 || episodeNow != t->episode) {\n    showduino_clear_txn_abort(t);\n    return SHOWDUINO_TXN_REJECT_EPISODE;\n  }\n  t->state = SHOWDUINO_TXN_VERIFY;",
     "  if (episodeNow == 0 || episodeNow != t->episode) {\n    showduino_clear_txn_abort(t);\n    return SHOWDUINO_TXN_REJECT_EPISODE;\n  }\n  t->state = SHOWDUINO_TXN_VERIFY;", None),
    ("K10 confirm episode not checked", "showduino_clear_txn.h",
     "  if (episodeNow == 0 || episodeNow != t->episode) {\n    showduino_clear_txn_abort(t);\n    return SHOWDUINO_TXN_REJECT_EPISODE;\n  }\n  t->state = SHOWDUINO_TXN_VERIFY;",
     "  (void)episodeNow;\n  t->state = SHOWDUINO_TXN_VERIFY;", None),
    ("K11 ASSERT does not cancel the transaction", "showduino_clear_txn.h",
     "  showduino_clear_txn_abort(t);\n  return SHOWDUINO_TXN_REJECT_SUPERSEDED;", "  return 0;", None),
    ("K12 fresh PRESSED not an immediate reject", "showduino_clear_txn.h",
     "    if (m[i].used && m[i].reportKnown && m[i].inputOpen && m[i].proofChalSeq != 0 &&",
     "    if (0 && m[i].used && m[i].reportKnown && m[i].inputOpen && m[i].proofChalSeq != 0 &&", None),
    ("K13 verification window never expires", "showduino_clear_txn.h",
     "  if ((uint32_t)(nowMs - t->verifyStartMs) > SHOWDUINO_CLEAR_VERIFY_WINDOW_MS) {",
     "  if (0) {", None),
    ("K14 cached proofs count (verifySeq ignored)", "showduino_clear_txn.h",
     "  const int d = showduino_station_clear_decision(m, n, ids, idCount, t->verifySeq, unknownBoot,",
     "  const int d = showduino_station_clear_decision(m, n, ids, idCount, 1u, unknownBoot,", None),
    ("K15 transaction opens without an episode", "showduino_clear_txn.h",
     "  if (!t || !rnd || episode == 0) return 0;", "  if (!t || !rnd) return 0;", None),
    ("K16 plain clear text from the UART treated as local", "showduino_clear_txn.h",
     "  if (isClear || isConfirm) return SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH;",
     "  if (isClear || isConfirm) return SHOWDUINO_CLEAR_ENTRY_LOCAL;", None),
    ("K17 Director messages accepted on USB", "showduino_clear_txn.h",
     "    if (isDclr || isConfirm || isClear) return SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE;", "", None),
    ("K18 re-press while latched waits for the heartbeat", "showduino_emergency_node.h",
     "  if (button_pressed && !wasPressed && m->latched) {\n    m->pending_assert = 1;\n    m->burst_left = (uint8_t)SHOWDUINO_EMERGENCY_BURST_COUNT;\n    m->next_tx_ms = 0;\n  }",
     "  if (button_pressed && !wasPressed && m->latched) {\n    (void)0;\n  }", None),
    ("K20 ACKed released station keeps heartbeating", "showduino_emergency_node.h",
     "  if (m->acked && !m->input_open && m->burst_left == 0) return 0;", "", None),
    ("K19 storage/overflow faults wait for the window", "showduino_clear_txn.h",
     "  if (d == SHOWDUINO_CLEAR_BLOCK_STORAGE || d == SHOWDUINO_CLEAR_BLOCK_OVERFLOW) {",
     "  if (0) {", None),
]

MUTANTS += [
    ("L01 lost latch record boots CLEAR (marker ignored)", "showduino_emergency_latch.h",
     "in.markerPresent ? SHOWDUINO_LATCH_READ_FAULT : SHOWDUINO_LATCH_READ_CLEAR;",
     "SHOWDUINO_LATCH_READ_CLEAR;", None),
    ("L02 key conflict check skips the set", "showduino_station_auth.h",
     "    if (showduino_auth_ct_equal(candidate, existing[k], SHOWDUINO_AUTH_KEY_BYTES)) return 1;",
     "", None),
    ("L03 key conflict check skips the last entry", "showduino_station_auth.h",
     "  for (int k = 0; existing && k < count; ++k) {",
     "  for (int k = 0; existing && k < count - 1; ++k) {", None),
    ("L04 all-zero key accepted", "showduino_station_auth.h",
     "  if (!nonzero) return 1;", "  (void)nonzero;", None),
    ("L05 recovery accepted from the Comms UART", "showduino_maintenance_recovery.h",
     "  if (!fromLocalUsb) return SHOWDUINO_RECOVER_REFUSE_SOURCE;", "  (void)fromLocalUsb;", 0),
    ("L06 blocker recovery without confirmation", "showduino_maintenance_recovery.h",
     "  if (!token || strcmp(token, SHOWDUINO_RECOVER_BLOCKERS_TOKEN) != 0) return SHOWDUINO_RECOVER_REFUSE_TOKEN;",
     "  (void)token;", None),
    ("L07 blocker recovery with GPIO25 open", "showduino_maintenance_recovery.h",
     "  if (physicalPressed) return SHOWDUINO_RECOVER_REFUSE_BUTTON;", "  (void)physicalPressed;", None),
    ("L08 blocker recovery when the state is known", "showduino_maintenance_recovery.h",
     "  if (!bootUnknown) return SHOWDUINO_RECOVER_REFUSE_NOT_NEEDED;", "  (void)bootUnknown;", None),
    ("L09 stored blockers dropped from the recovered set", "showduino_maintenance_recovery.h",
     "  for (int i = 0; stored && i < storedCount; ++i) {",
     "  for (int i = 0; stored && i < 0 && storedCount; ++i) {", None),
    ("L10 provisioned stations dropped from the recovered set", "showduino_maintenance_recovery.h",
     "  for (int i = 0; provIds && i < provCount; ++i) {",
     "  for (int i = 0; provIds && i < 0 && provCount; ++i) {", None),
    ("L11 overflow truncates instead of refusing", "showduino_maintenance_recovery.h",
     "    if (*count >= cap) { *count = 0; return SHOWDUINO_RECOVER_REFUSE_OVERFLOW; }\n    memset",
     "    if (*count >= cap) { return SHOWDUINO_RECOVER_OK; }\n    memset", None),
    ("L12 counter recovery without confirmation", "showduino_maintenance_recovery.h",
     "  if (!token || strcmp(token, SHOWDUINO_RECOVER_COUNTER_TOKEN) != 0) return SHOWDUINO_RECOVER_REFUSE_TOKEN;",
     "  (void)token;", None),
    ("L13 counter recovery from the Comms UART", "showduino_maintenance_recovery.h",
     "  if (!fromLocalUsb) return SHOWDUINO_RECOVER_REFUSE_SOURCE;", "  (void)fromLocalUsb;", 1),
]

# Mutants shown to be equivalent, with the reason. Must stay empty unless argued.
EQUIVALENT = {}


def nth_replace(text, old, new, occ):
    if occ is None:
        if text.count(old) != 1:
            return None
        return text.replace(old, new, 1)
    idx = -1
    for _ in range(occ + 1):
        idx = text.find(old, idx + 1)
        if idx < 0:
            return None
    return text[:idx] + new + text[idx + len(old):]


def run_suites(inc, work):
    for i, src in enumerate(SUITES):
        exe = os.path.join(work, "t%d" % i)
        c = subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I" + inc,
                            os.path.join(ROOT, src), "-lcrypto", "-o", exe],
                           capture_output=True, text=True)
        if c.returncode != 0:
            return "INVALID", c.stderr.strip().splitlines()[:3]
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode != 0:
            fails = [l for l in r.stdout.splitlines() if l.startswith("FAIL")]
            return "KILLED", fails[:2]
    return "SURVIVED", []


def main():
    work = tempfile.mkdtemp(prefix="sd_mut_")
    inc = os.path.join(work, "protocol")
    bad = 0
    try:
        shutil.copytree(os.path.join(ROOT, "protocol"), inc)
        status, _ = run_suites(inc, work)
        if status != "SURVIVED":
            print("BASELINE FAILS: fix the suites before running mutations")
            return 2
        for mid, fname, old, new, occ in MUTANTS:
            shutil.rmtree(inc)
            shutil.copytree(os.path.join(ROOT, "protocol"), inc)
            path = os.path.join(inc, fname)
            text = open(path).read()
            mutated = nth_replace(text, old, new, occ)
            if mutated is None:
                print("NOMATCH   %s (pattern not found: the mutant list is stale)" % mid)
                bad += 1
                continue
            open(path, "w").write(mutated)
            status, detail = run_suites(inc, work)
            if status == "SURVIVED" and mid in EQUIVALENT:
                status = "EQUIV"
            print("%-9s %s" % (status, mid))
            for d in detail:
                print("            " + d)
            if status in ("SURVIVED", "INVALID"):
                bad += 1
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("\n%d mutants, %d problems" % (len(MUTANTS), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
