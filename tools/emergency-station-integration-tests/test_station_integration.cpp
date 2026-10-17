/*
 * Emergency station integration tests (MISSION 001H, 001I, 001J).
 *
 * Exercised, using the REAL protocol code:
 *   C3 side : ShowduinoEmergencyMachine (protocol/showduino_emergency_node.h) and the
 *             station authentication policy (protocol/showduino_station_responder.h).
 *   P4 side : showduino_station_interpret_line() and showduino_station_clear_decision()
 *             (protocol/showduino_station_model.h), the challenge table and command
 *             counter (protocol/showduino_station_auth.h), blocker encode/decode
 *             (protocol/showduino_station_blockers.h).
 *   Crypto  : HMAC-SHA256 from OpenSSL (mbedtls on the ESP32), 128-bit tags.
 *
 * Simulated: the S3 Comms relay (in-order delivery, with test-controlled drop,
 * record, replay and forgery; it holds no keys), NVS / Preferences, time, and the
 * P4 global latch and Director confirm.
 *
 * NOT covered: the Arduino wrappers (EmergencyProtocol.cpp, EmergencyNodeLink.cpp,
 * ProtocolBridge.cpp), ESP-NOW, UART, GPIO, real flash. Those need bench tests.
 *
 * Build: g++ -std=c++17 -Wall -Wextra -Werror -Iprotocol test_station_integration.cpp -lcrypto
 */
#include "showduino_emergency_node.h"
#include "showduino_station_blockers.h"
#include "showduino_station_model.h"
#include "showduino_station_responder.h"
#include "showduino_director_auth.h"
#include "showduino_clear_txn.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static int gFails = 0;
static int gPass = 0;
static void expect(bool ok, const char *msg) {
  if (ok) { std::printf("PASS  %s\n", msg); gPass++; }
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

/* ---- HMAC-SHA256 (OpenSSL), same function type the firmware wraps mbedtls in ---- */
static int hmacSha256(const uint8_t *k, size_t kl, const uint8_t *m, size_t ml, uint8_t out[32]) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MAC *mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
  if (!mac) return -1;
  EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
  OSSL_PARAM params[2];
  char digest[] = "SHA256";
  params[0] = OSSL_PARAM_construct_utf8_string("digest", digest, 0);
  params[1] = OSSL_PARAM_construct_end();
  size_t len = 0;
  int rc = -1;
  if (ctx && EVP_MAC_init(ctx, k, kl, params) && EVP_MAC_update(ctx, m, ml) &&
      EVP_MAC_final(ctx, out, &len, 32) && len == 32) {
    rc = 0;
  }
  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(mac);
  return rc;
#else
  unsigned int len = 0;
  return HMAC(EVP_sha256(), k, (int)kl, m, ml, out, &len) && len == 32 ? 0 : -1;
#endif
}

/* Deterministic test RNG. Never used on hardware (the P4 uses esp_fill_random). */
static uint64_t gRng = 0x0123456789ABCDEFull;
static int testRand(uint8_t *out, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
    out[i] = (uint8_t)(gRng >> 33);
  }
  return 0;
}

struct Key { uint8_t b[SHOWDUINO_AUTH_KEY_BYTES]; };
static Key makeKey(uint8_t seed) {
  Key k;
  for (int i = 0; i < SHOWDUINO_AUTH_KEY_BYTES; ++i) k.b[i] = (uint8_t)(seed * 31 + i * 7 + 1);
  return k;
}

/* ================= C3 Emergency Node: real machine + real responder ================= */
struct C3 {
  ShowduinoEmergencyMachine m{};
  ShowduinoStationAuthState st{};
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
  bool pressed = false;
  bool legacy = false;        /* pre-001J firmware: no key, honours unauthenticated commands */
  Key key{};
  bool keyValid = false;
  uint32_t nvsCounter = 0;    /* persisted last accepted command counter */
  int failCounterWrites = 0;
  int assertsSent = 0;
  int rejectedUnauth = 0;
  int rejectedCmd = 0;
  int lastCmdResult = 0;
  uint32_t lastNow = 0;

  C3(const char *idIn, const Key *k, bool startPressed, bool isLegacy = false) {
    std::snprintf(id, sizeof(id), "%s", idIn);
    std::snprintf(name, sizeof(name), "%s", "TEST");
    legacy = isLegacy;
    if (k) { key = *k; keyValid = true; }
    pressed = startPressed;
    boot();
  }

  /* Power-up. RAM state is lost; the key and the command counter come from NVS. */
  void boot() {
    showduino_emergency_machine_init(&m, pressed ? 1 : 0);
    showduino_emergency_machine_set_radio(&m, 1);
    showduino_responder_boot(&st, key.b, (keyValid && !legacy) ? 1 : 0, nvsCounter, m.latched);
  }

  static int persistCb(void *ctx, uint32_t v) {
    C3 *c = static_cast<C3 *>(ctx);
    if (c->failCounterWrites > 0) { c->failCounterWrites--; return 0; }
    c->nvsCounter = v;
    return 1;
  }

  /* One service pass, mirroring EmergencyProtocol.cpp emergencyProtocolService(). */
  std::vector<std::string> tick(uint32_t now) {
    lastNow = now;
    std::vector<std::string> out;
    showduino_emergency_machine_input(&m, pressed ? 1 : 0);
    showduino_emergency_machine_set_radio(&m, 1);
    showduino_responder_track_latch(&st, m.latched);
    if (showduino_emergency_machine_want_tx(&m, now)) {
      char cmd[48];
      showduino_emergency_format_assert(id, name, cmd, sizeof(cmd));
      out.push_back(cmd);
      assertsSent++;
      showduino_emergency_machine_sent(&m, now);
    }
    return out;
  }

  /* ANNOUNCE as the station formats it (display only on a 001J P4). */
  std::string announce() const {
    ShowduinoEmergencyAnnounce an{};
    std::snprintf(an.mac, sizeof(an.mac), "%s", "AA:BB:CC:DD:EE:01");
    std::snprintf(an.firmware, sizeof(an.firmware), "%s", "1.0.0");
    std::snprintf(an.state, sizeof(an.state), "%s", showduino_emergency_state_name(m.state));
    std::snprintf(an.id, sizeof(an.id), "%s", id);
    std::snprintf(an.name, sizeof(an.name), "%s", name);
    an.input_open = m.input_open;
    an.latched = m.latched;
    an.acked = m.acked;
    char buf[160];
    showduino_emergency_format_announce(&an, buf, sizeof(buf));
    return buf;
  }

  /* Local USB maintenance GLOBAL:OBSERVED (physical access): station only. */
  void localUsbObserve() {
    showduino_emergency_machine_global_observed(&m);
    showduino_responder_track_latch(&st, m.latched);
  }

  /* Downlink line from the relay, mirroring EmergencyProtocol.cpp emergencyProtocolApply(). */
  std::vector<std::string> receive(const std::string &line) {
    std::vector<std::string> out;
    showduino_responder_track_latch(&st, m.latched);
    if (!std::strncmp(line.c_str(), "CHAL:", 5)) {
      char buf[SHOWDUINO_NODE_COMMAND_MAX];
      if (showduino_responder_answer_chal(&st, id, m.input_open, m.latched, line.c_str(),
                                          hmacSha256, buf, sizeof(buf))) {
        out.push_back(buf);
      }
      return out;
    }
    if (!std::strncmp(line.c_str(), "CMD:", 4)) {
      uint32_t ep = 0;
      lastCmdResult = showduino_responder_accept_cmd(&st, id, m.latched, line.c_str(), hmacSha256,
                                                     persistCb, this, &ep);
      if (lastCmdResult == SHOWDUINO_RESP_ACK) {
        showduino_emergency_machine_ack(&m);
      } else if (lastCmdResult == SHOWDUINO_RESP_GCL) {
        showduino_emergency_machine_global_observed(&m);
        showduino_responder_track_latch(&st, m.latched);
      } else {
        rejectedCmd++;
      }
      return out;
    }
    /* Unauthenticated ACK / GLOBAL:OBSERVED / EMERGENCY:CLEAR. */
    const ShowduinoEmergencyNodeCmd c = showduino_emergency_parse_command(line.c_str());
    if (c == SHOWDUINO_ESTOP_CMD_ACK_LATCHED || c == SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED) {
      if (legacy) {
        /* Pre-001J behaviour, kept only to model mixed installations. */
        if (c == SHOWDUINO_ESTOP_CMD_ACK_LATCHED) showduino_emergency_machine_ack(&m);
        else showduino_emergency_machine_global_observed(&m);
      } else {
        rejectedUnauth++;
      }
    }
    return out;
  }
};

/* ---------------- Stored blocker record (simulated NVS) ---------------- */
struct StoreSim {
  std::string value;
  bool valid = false;
  int failWrites = 0;
  bool save(const std::string &enc) {
    if (failWrites > 0) { failWrites--; return false; }
    value = enc;
    valid = true;
    return true;
  }
};

/* ================= P4 Stage Engine + untrusted S3 relay ================= */
struct P4 {
  ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES]{};
  char ids[SHOWDUINO_EMERGENCY_NODE_MAX_NODES][SHOWDUINO_BLOCKER_ID_CHARS + 1]{};
  Key keys[SHOWDUINO_EMERGENCY_NODE_MAX_NODES]{};
  int keyCount = 0;

  ShowduinoAuthChalTable chal{};
  ShowduinoAuthCounter ctr{};
  uint32_t nvsCounterLimit = 1;   /* persisted counter-block limit (shared by commands and episodes) */
  int failCounterWrites = 0;
  bool counterFault = false;      /* an episode or command counter could not be made durable */
  uint32_t episode = 0;
  uint32_t now = 10000;

  /* MISSION 001K: clearance transaction, physical input, Director credential. */
  ShowduinoClearTxn txn{};
  int lastTxnResult = -1;
  bool txnSuperseded = false;
  bool physicalPressed = false;       /* P4 GPIO25 loop open */
  ShowduinoDirKeyRec dirRec{};
  uint32_t dirCtrNvs = 0;             /* persisted last accepted Director counter */
  int failDirWrites = 0;
  int lastDirResult = -1;
  std::string lastReply;
  struct Director *director = nullptr;
  bool dropToDirector = false;

  bool globalLatched = false;
  int globalTriggers = 0;
  int acksSent = 0;
  int clearsAccepted = 0;
  int clearsRefused = 0;
  int lastDecision = -1;
  bool tableOverflow = false;

  StoreSim store;
  bool persistDirty = false;
  bool persistValid = false;
  bool bootUnknown = false;
  std::string persistedEnc;

  /* The S3 relay. It forwards in order unless a test tells it otherwise. */
  std::vector<C3 *> stations;
  std::set<std::string> dropUp, dropDown;   /* by station ID */
  std::string dropDownMatch;                /* drop downlink lines containing this */
  bool dropAllUp = false;
  std::vector<std::string> upLog, downLog;
  std::vector<std::string> extraDown;       /* lines the relay injects (forgery) */

  P4() { showduino_auth_counter_boot(&ctr, nvsCounterLimit); }

  /* -- key table (provisioning is local to the P4; the relay never sees keys) -- */
  void provision(const char *id, const Key &k) {
    for (int i = 0; i < keyCount; ++i) {
      if (!std::strcmp(ids[i], id)) { keys[i] = k; return; }
    }
    std::snprintf(ids[keyCount], sizeof(ids[keyCount]), "%s", id);
    keys[keyCount++] = k;
  }
  void revoke(const char *id) {
    for (int i = 0; i < keyCount; ++i) {
      if (!std::strcmp(ids[i], id)) {
        for (int j = i; j + 1 < keyCount; ++j) {
          std::memcpy(ids[j], ids[j + 1], sizeof(ids[j]));
          keys[j] = keys[j + 1];
        }
        keyCount--;
        std::memset(ids[keyCount], 0, sizeof(ids[keyCount]));
        return;
      }
    }
  }
  bool isProvisioned(const char *id) const {
    for (int i = 0; i < keyCount; ++i) if (!std::strcmp(ids[i], id)) return true;
    return false;
  }
  static int keyFor(void *ctx, const char *id, uint8_t out[SHOWDUINO_AUTH_KEY_BYTES]) {
    P4 *p = static_cast<P4 *>(ctx);
    for (int i = 0; i < p->keyCount; ++i) {
      if (showduino_emergency_id_equal(p->ids[i], id)) {
        std::memcpy(out, p->keys[i].b, SHOWDUINO_AUTH_KEY_BYTES);
        return i;
      }
    }
    return -1;
  }
  static int persistCtr(void *ctx, uint32_t v) {
    P4 *p = static_cast<P4 *>(ctx);
    if (p->failCounterWrites > 0) { p->failCounterWrites--; return 0; }
    p->nvsCounterLimit = v;
    return 1;
  }
  /* Mirrors EmergencyNodeLink nextCounter(): a failed block write sets the fault,
   * a later successful one clears it. */
  bool nextCounter(uint32_t *out) {
    if (!showduino_auth_counter_next(&ctr, persistCtr, this, out)) {
      counterFault = true;
      return false;
    }
    counterFault = false;
    return true;
  }
  /* Mirrors emergencyNodeLinkLoop(): while latched without a durable episode, retry. */
  void ensureEpisode() {
    if (!globalLatched || episode != 0) return;
    uint32_t e = 0;
    if (nextCounter(&e)) {
      episode = e;
      showduino_auth_chal_reset(&chal);
      for (auto &m : t) m.ackedReport = 0;
    }
  }

  C3 *byId(const char *id) {
    for (C3 *s : stations) if (!std::strcmp(s->id, id)) return s;
    return nullptr;
  }

  /* ---- relay ---- */
  void up(C3 *from, const std::string &line) {
    upLog.push_back(line);
    if (dropAllUp) return;
    if (from && dropUp.count(from->id)) return;
    if (holdUp && !std::strncmp(line.c_str(), "AUTH:", 5)) { heldUp.push_back({from, line}); return; }
    if (dropAssertUp && showduino_emergency_is_assert(line.c_str())) return;
    deliver(line);
  }
  bool dropAssertUp = false;
  void down(C3 *to, const std::string &line) {
    downLog.push_back(line);
    if (dropDown.count(to->id)) return;
    if (!dropDownMatch.empty() && line.find(dropDownMatch) != std::string::npos) return;
    for (const std::string &r : to->receive(line)) up(to, r);
  }

  /* ---- P4 behaviour ---- */
  /* MISSION 001F ordering: the RAM latch first; episode allocation (storage) after. */
  void engage() {
    globalLatched = true;
    globalTriggers++;
    uint32_t e = 0;
    episode = nextCounter(&e) ? e : 0;
    showduino_auth_chal_reset(&chal);
    for (auto &m : t) m.ackedReport = 0;
  }

  bool sendCmd(const char *id, const char *op, uint32_t ep) {
    const int ki = [&] { for (int i = 0; i < keyCount; ++i) if (!std::strcmp(ids[i], id)) return i; return -1; }();
    C3 *s = byId(id);
    if (ki < 0 || !s || ep == 0) return false;
    uint32_t c = 0;
    if (!nextCounter(&c)) return false;
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    if (!showduino_auth_format_cmd(id, c, op, ep, keys[ki].b, SHOWDUINO_AUTH_KEY_BYTES, hmacSha256,
                                   line, sizeof(line))) {
      return false;
    }
    down(s, line);
    return true;
  }

  ShowduinoStationResult deliver(const std::string &line, bool *handled = nullptr) {
    ShowduinoStationAuthPolicy pol{hmacSha256, &chal, episode, now, keyFor, this};
    ShowduinoStationResult res{};
    const int h = showduino_station_interpret_line(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES,
                                                   line.c_str(), &pol, &res);
    if (handled) *handled = h != 0;
    if (!h) return res;
    if (res.tableFull) tableOverflow = true;
    /* MISSION 001K: any ASSERT (or a verified latched report that re-engages) ends a
     * pending clear transaction. Mirrors EmergencyNodeLink. */
    if (res.kind == SHOWDUINO_STATION_LINE_ASSERT || (res.globalLatch && !globalLatched)) {
      if (showduino_clear_txn_on_assert(&txn)) txnSuperseded = true;
    }
    if (res.globalLatch && !globalLatched) engage();
    ensureEpisode();
    persist();
    /* Authenticated ACK so the station learns the episode. Sent for an ASSERT, and
     * for a verified AUTH that shows the station latched but not holding this episode. */
    /* Mirrors EmergencyNodeLink ackIfDue(): not when the station's verified report
     * already shows it holding this episode. */
    if (globalLatched && res.ackId[0] && isProvisioned(res.ackId) && res.slot >= 0) {
      const ShowduinoStationModel &m = t[res.slot];
      const bool holds = m.reportKnown && m.ackedReport;
      const bool needAck = !holds && (res.kind == SHOWDUINO_STATION_LINE_ASSERT ||
                                      (res.verified && m.announcedLatched));
      if (needAck) { sendCmd(res.ackId, "ACK", episode); acksSent++; }
    }
    return res;
  }

  /* One challenge round to every station through the relay. */
  void challengeRound() {
    char nonce[SHOWDUINO_AUTH_NONCE_HEX + 1];
    if (!showduino_auth_chal_issue(&chal, now, episode, testRand, nonce)) return;
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    showduino_auth_format_chal(episode, nonce, line, sizeof(line));
    for (C3 *s : stations) down(s, line);
  }

  void pump(C3 &s) { for (const std::string &l : s.tick(now)) up(&s, l); }
  /* Continuous time: stations serviced every 50 ms, challenges at the latched or
   * idle cadence, as the firmware loops do. */
  void runFor(uint32_t ms) {
    uint32_t lastChal = now;
    for (uint32_t t0 = 0; t0 < ms; t0 += 50) {
      advance(50);
      pumpAll();
      const uint32_t period = globalLatched ? SHOWDUINO_STATION_CHAL_LATCHED_MS : SHOWDUINO_STATION_CHAL_IDLE_MS;
      if (now - lastChal >= period) { challengeRound(); lastChal = now; }
    }
  }
  void pumpAll() { for (C3 *s : stations) pump(*s); }
  void advance(uint32_t ms) { now += ms; }

  /* MISSION 001K: every clear runs the shared transaction (showduino_clear_txn.h):
   * a confirm starts verification, only answers to challenges issued after it count,
   * any ASSERT or fresh PRESSED aborts, missing answers fail at the window. */
  static int mapTxn(int r, const ShowduinoClearTxn &t) {
    if (r == SHOWDUINO_TXN_REJECT_PRESSED || r == SHOWDUINO_TXN_REJECT_SUPERSEDED) return SHOWDUINO_CLEAR_BLOCK_STATION;
    if (r == SHOWDUINO_TXN_REJECT_UNVERIFIED || r == SHOWDUINO_TXN_REJECT_BLOCKED) return t.blockDecision;
    if (r == SHOWDUINO_TXN_REJECT_EPISODE || r == SHOWDUINO_TXN_REJECT_NOT_OPEN) return SHOWDUINO_CLEAR_BLOCK_STORAGE;
    return SHOWDUINO_CLEAR_BLOCK_STATION;
  }
  bool refuse(int decision, int txnResult, const char *reply) {
    lastDecision = decision;
    lastTxnResult = txnResult;
    lastReply = reply;
    clearsRefused++;
    return false;
  }

  /* Local USB maintenance clear (EMERGENCY:CLEAR on the P4 console). */
  bool directorClear() { return localClear(); }
  bool localClear() {
    if (showduino_clear_entry(1, "EMERGENCY:CLEAR") != SHOWDUINO_CLEAR_ENTRY_LOCAL) return false;
    if (!globalLatched) return refuse(SHOWDUINO_CLEAR_BLOCK_STATION, -1, "NOT_LATCHED");
    if (physicalPressed) return refuse(SHOWDUINO_CLEAR_BLOCK_STATION, -2, "BUTTON_ACTIVE");
    if (!showduino_clear_txn_open(&txn, SHOWDUINO_CLEAR_BY_LOCAL_USB, episode, now, testRand)) {
      return refuse(SHOWDUINO_CLEAR_BLOCK_STORAGE, SHOWDUINO_TXN_REJECT_EPISODE, "STATION_STORAGE");
    }
    return confirmAndVerify();
  }

  /* Confirm accepted: verification, then apply. */
  bool confirmAndVerify() {
    txnSuperseded = false;
    if (physicalPressed) {
      showduino_clear_txn_abort(&txn);
      return refuse(SHOWDUINO_CLEAR_BLOCK_STATION, -2, "BUTTON_ACTIVE");
    }
    int r = showduino_clear_txn_begin_verify(&txn, episode, now, showduino_auth_chal_next_seq(&chal));
    if (r != SHOWDUINO_TXN_PENDING) return refuse(mapTxn(r, txn), r, showduino_clear_txn_result_name(r));
    challengeRound();
    for (int guard = 0;; ++guard) {
      if (guard > 64) { r = -99; break; }   /* verification never ended: a test failure */
      if (txnSuperseded) { r = SHOWDUINO_TXN_REJECT_SUPERSEDED; break; }
      r = showduino_clear_txn_evaluate(&txn, t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, ids, keyCount,
                                       episode, now, bootUnknown ? 1 : 0, persistDirty ? 1 : 0,
                                       counterFault ? 1 : 0, tableOverflow ? 1 : 0, 1);
      if (r != SHOWDUINO_TXN_PENDING) break;
      advance(SHOWDUINO_CLEAR_VERIFY_RECHAL_MS);
      if (showduino_clear_txn_want_rechallenge(&txn, now)) challengeRound();
    }
    if (r != SHOWDUINO_TXN_ACCEPT) return refuse(mapTxn(r, txn), r, showduino_clear_txn_result_name(r));
    if (physicalPressed) return refuse(SHOWDUINO_CLEAR_BLOCK_STATION, -2, "BUTTON_ACTIVE");
    /* Reserve one GCL counter per station before anything changes. */
    std::vector<uint32_t> reserved;
    for (int i = 0; i < keyCount; ++i) {
      uint32_t c = 0;
      if (!nextCounter(&c)) return refuse(SHOWDUINO_CLEAR_BLOCK_STORAGE, -3, "STATION_STORAGE");
      reserved.push_back(c);
    }
    lastDecision = SHOWDUINO_CLEAR_OK;
    lastTxnResult = SHOWDUINO_TXN_ACCEPT;
    lastReply = "EMERGENCY:CLEAR_OK";
    globalLatched = false;
    clearsAccepted++;
    /* An accepted clear lifts the restriction on a Director enrolled mid-emergency. */
    dirRec.restricted = 0;
    const uint32_t ended = episode;
    showduino_station_model_end_episode(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
    persist();
    episode = 0;
    showduino_auth_chal_reset(&chal);
    for (int i = 0; i < keyCount; ++i) {
      C3 *s = byId(ids[i]);
      if (!s) continue;
      char line[SHOWDUINO_NODE_COMMAND_MAX];
      if (showduino_auth_format_cmd(ids[i], reserved[i], "GCL", ended, keys[i].b,
                                    SHOWDUINO_AUTH_KEY_BYTES, hmacSha256, line, sizeof(line))) {
        down(s, line);
      }
    }
    return true;
  }

  /* ---- Director path: Director -> S3 -> P4 (fromComms) and P4 -> S3 -> Director ---- */
  static int dirPersistCb(void *ctx, uint32_t v) {
    P4 *p = static_cast<P4 *>(ctx);
    if (p->failDirWrites > 0) { p->failDirWrites--; return 0; }
    p->dirCtrNvs = v;
    return 1;
  }
  void enrollDirector(const char *id, const Key &k) {
    dirRec = ShowduinoDirKeyRec{};
    dirRec.present = 1;
    std::snprintf(dirRec.id, sizeof(dirRec.id), "%s", id);
    std::memcpy(dirRec.key, k.b, sizeof(dirRec.key));
    dirRec.restricted = globalLatched ? 1 : 0;   /* EmergencyDirectorKey rule */
    dirRec.lastCounter = 0;
    dirCtrNvs = 0;
  }
  void toDirector(const std::string &line);
  void publishEpisode() {
    char buf[64];
    showduino_dir_format_episode(globalLatched ? episode : 0, buf, sizeof(buf));
    toDirector(buf);
  }
  /* A line arriving on the P4 UART (Comms source). Mirrors the sketch dispatch. */
  int fromComms(const std::string &line) {
    const int e = showduino_clear_entry(0, line.c_str());
    if (e == SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH) {
      lastReply = "EMERGENCY:CLEAR:REJECTED:AUTH_REQUIRED";
      clearsRefused++;
      return e;
    }
    if (e == SHOWDUINO_CLEAR_ENTRY_CANCEL) { showduino_clear_txn_abort(&txn); return e; }
    if (e != SHOWDUINO_CLEAR_ENTRY_DIRECTOR) return e;
    ShowduinoDirMsg m;
    lastDirResult = showduino_dir_accept(&dirRec, line.c_str(), globalLatched ? episode : 0,
                                         showduino_clear_txn_open_nonce(&txn), hmacSha256,
                                         dirPersistCb, this, &m);
    if (lastDirResult != SHOWDUINO_DIR_OK) {
      lastReply = std::string("EMERGENCY:CLEAR:REJECTED:") + showduino_dir_result_name(lastDirResult);
      clearsRefused++;
      return e;
    }
    if (m.op == SHOWDUINO_DIR_OP_REQ) {
      if (physicalPressed) { refuse(SHOWDUINO_CLEAR_BLOCK_STATION, -2, "BUTTON_ACTIVE"); return e; }
      if (!showduino_clear_txn_open(&txn, SHOWDUINO_CLEAR_BY_DIRECTOR, episode, now, testRand)) {
        refuse(SHOWDUINO_CLEAR_BLOCK_STORAGE, SHOWDUINO_TXN_REJECT_EPISODE, "STATION_STORAGE");
        return e;
      }
      char buf[64];
      showduino_dir_format_txn_notice(episode, txn.nonceHex, buf, sizeof(buf));
      toDirector(buf);
      return e;
    }
    confirmAndVerify();
    return e;
  }

  /* Relay holding station answers (models a delaying S3). */
  std::vector<std::pair<C3 *, std::string>> heldUp;
  bool holdUp = false;
  void releaseHeld() {
    auto h = heldUp;
    heldUp.clear();
    for (auto &x : h) deliver(x.second);
  }

  void timeout(const char *id) {
    const int slot = slotOf(id);
    if (slot >= 0) showduino_station_model_on_timeout(&t[slot]);
  }
  int slotOf(const char *id) const {
    return showduino_station_model_find_id(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, id);
  }
  int blocking() const {
    return showduino_station_model_count_blocking(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
  }

  /* Mirrors emergencyNodeLinkPersistBlockers(). */
  bool persist() {
    ShowduinoBlocker list[SHOWDUINO_BLOCKER_MAX];
    int n = 0;
    if (!showduino_station_model_collect_required(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, list,
                                                  SHOWDUINO_BLOCKER_MAX, &n)) {
      persistDirty = true;
      return false;
    }
    char buf[SHOWDUINO_BLOCKER_BUF];
    if (showduino_blocker_encode(list, (uint8_t)n, buf, sizeof(buf)) < 0) {
      persistDirty = true;
      return false;
    }
    const std::string enc(buf);
    if (persistValid && !persistDirty && enc == persistedEnc) return true;
    if (!store.save(enc)) { persistDirty = true; return false; }
    persistedEnc = enc;
    persistValid = true;
    persistDirty = false;
    return true;
  }

  /* After a P4 reboot while latched: a new episode is allocated and every provisioned
   * station is sent an ACK so it learns it (mirrors emergencyNodeLinkBegin()). */
  void ackSweep() {
    if (!globalLatched) return;
    for (int i = 0; i < keyCount; ++i) {
      C3 *s = byId(ids[i]);
      if (s && s->m.latched) sendCmd(ids[i], "ACK", episode);
    }
  }
};

/* ================= Director (real signer) ================= */
struct Director {
  ShowduinoDirSigner s{};
  uint32_t nvsCtr = 1;            /* persisted counter-block limit */
  int failWrites = 0;
  P4 *p4 = nullptr;
  std::vector<std::string> sent;  /* everything the relay saw from the Director */
  int signFailures = 0;

  Director(const char *id, const Key &k) { enroll(id, k); }
  void enroll(const char *id, const Key &k) {
    s = ShowduinoDirSigner{};
    s.provisioned = 1;
    std::snprintf(s.id, sizeof(s.id), "%s", id);
    std::memcpy(s.key, k.b, sizeof(s.key));
    showduino_auth_counter_boot(&s.ctr, nvsCtr);
  }
  /* Power cycle: RAM (episode, txn) lost, key and counter limit kept. */
  void reboot() {
    const uint8_t prov = s.provisioned;
    char id[sizeof(s.id)];
    std::memcpy(id, s.id, sizeof(id));
    uint8_t key[32];
    std::memcpy(key, s.key, 32);
    s = ShowduinoDirSigner{};
    s.provisioned = prov;
    std::memcpy(s.id, id, sizeof(id));
    std::memcpy(s.key, key, 32);
    showduino_auth_counter_boot(&s.ctr, nvsCtr);
  }
  static int persistCb(void *ctx, uint32_t v) {
    Director *d = static_cast<Director *>(ctx);
    if (d->failWrites > 0) { d->failWrites--; return 0; }
    d->nvsCtr = v;
    return 1;
  }
  void onNotice(const std::string &line) {
    uint32_t e = 0;
    if (showduino_dir_parse_episode(line.c_str(), &e)) s.episode = e;
    showduino_dir_parse_txn_notice(line.c_str(), &s.txnEpisode, s.txnHex);
  }
  /* Touch "E-CLEAR". */
  bool tapClear() {
    char buf[128];
    if (!showduino_dir_sign_req(&s, hmacSha256, persistCb, this, buf, sizeof(buf))) { signFailures++; return false; }
    sent.push_back(buf);
    if (p4) p4->fromComms(buf);
    return true;
  }
  /* Touch "Confirm" in the clear dialog. */
  bool tapConfirm() {
    char buf[128];
    if (!showduino_dir_sign_cfm(&s, hmacSha256, persistCb, this, buf, sizeof(buf))) { signFailures++; return false; }
    sent.push_back(buf);
    if (p4) p4->fromComms(buf);
    return true;
  }
};

void P4::toDirector(const std::string &line) {
  if (director && !dropToDirector) director->onNotice(line);
}

/* Reboot: a new P4 with the same NVS (keys, counter limit, blocker record). */
static void rebootP4(const P4 &old, P4 &fresh, bool latched = true) {
  for (int i = 0; i < old.keyCount; ++i) fresh.provision(old.ids[i], old.keys[i]);
  fresh.nvsCounterLimit = old.nvsCounterLimit;
  showduino_auth_counter_boot(&fresh.ctr, fresh.nvsCounterLimit);
  fresh.stations = old.stations;
  fresh.now = old.now + 3000;
  fresh.store = old.store;
  fresh.globalLatched = latched;
  if (latched) {
    uint32_t e = 0;
    fresh.episode = fresh.nextCounter(&e) ? e : 0;
  }
  ShowduinoBlocker list[SHOWDUINO_BLOCKER_MAX];
  uint8_t n = 0;
  if (showduino_blocker_decode(old.store.value.c_str(), list, &n) != SHOWDUINO_BLOCKER_OK) {
    fresh.bootUnknown = latched;
    return;
  }
  for (uint8_t i = 0; i < n; ++i) {
    const int slot = showduino_station_model_alloc(fresh.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
    if (slot >= 0) showduino_station_model_restore(&fresh.t[slot], list[i].id, list[i].mac);
  }
  fresh.persistedEnc = old.store.value;
  fresh.persistValid = old.store.valid;
  /* Director credential and its last accepted counter come back from NVS. */
  fresh.dirRec = old.dirRec;
  fresh.dirRec.lastCounter = old.dirCtrNvs;
  fresh.dirCtrNvs = old.dirCtrNvs;
  fresh.director = old.director;
  if (fresh.director) fresh.director->p4 = &fresh;
}

/* A valid-looking AUTH forged without the key (random tag). */
static std::string forgedAuth(const char *id, uint32_t episode, const char *nonce) {
  Key wrong = makeKey(250);
  char buf[SHOWDUINO_NODE_COMMAND_MAX];
  showduino_auth_format_auth(id, episode, nonce, 0, 0, 0, wrong.b, SHOWDUINO_AUTH_KEY_BYTES,
                             hmacSha256, buf, sizeof(buf));
  return buf;
}

/* Last CHAL nonce the relay carried. */
static std::string lastNonce(const P4 &p) {
  for (auto it = p.downLog.rbegin(); it != p.downLog.rend(); ++it) {
    ShowduinoAuthChalMsg ch;
    if (showduino_auth_parse_chal(it->c_str(), &ch)) return ch.nonceHex;
  }
  return std::string(16, '0');
}
static std::string lastUp(const P4 &p, const char *prefix) {
  for (auto it = p.upLog.rbegin(); it != p.upLog.rend(); ++it) {
    if (!std::strncmp(it->c_str(), prefix, std::strlen(prefix))) return *it;
  }
  return "";
}
static std::string lastDown(const P4 &p, const char *prefix) {
  for (auto it = p.downLog.rbegin(); it != p.downLog.rend(); ++it) {
    if (it->find(prefix) != std::string::npos) return *it;
  }
  return "";
}

int main() {
  const Key KA = makeKey(1), KB = makeKey(2), KC = makeKey(3), KA2 = makeKey(9);

  /* ===== S1: legitimate press -> latch -> ACK -> release -> proof -> clear -> GCL ===== */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.challengeRound();
    const int s = p4.slotOf("ESTOP-01");
    expect(s >= 0 && p4.t[s].reportKnown && !p4.t[s].inputOpen && !p4.globalLatched,
           "S1a idle station proves RELEASED with a verified AUTH; no latch");
    a.pressed = true; p4.pump(a);
    expect(p4.globalLatched && p4.globalTriggers == 1 && p4.episode != 0,
           "S1b press: bare ASSERT latches the P4 immediately and opens an episode");
    expect(a.m.acked == 1 && a.st.latchedEpisode == p4.episode,
           "S1c station accepts the authenticated ACK and learns the episode");
    {
      p4.challengeRound();                       /* verified report now shows A=1 */
      const int before = p4.acksSent;
      const uint32_t flashBefore = a.nvsCounter;
      for (int i = 0; i < 5; ++i) { p4.advance(SHOWDUINO_EMERGENCY_HEARTBEAT_MS + 1); p4.pump(a); }
      expect(a.assertsSent >= 5 && p4.acksSent == before && a.nvsCounter == flashBefore,
             "S1c2 heartbeat ASSERTs from an ACKed station cost no command and no flash write");
    }
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S1d clear refused while the station is PRESSED");
    a.pressed = false; p4.pump(a); p4.advance(500);
    expect(p4.directorClear() && !p4.globalLatched && p4.clearsAccepted == 1,
           "S1e released + fresh proof: Director clear accepted");
    expect(a.m.latched == 0 && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "S1f station returns to NORMAL only after the authenticated GCL");
    p4.advance(3000); p4.pump(a);
    expect(!p4.globalLatched && a.assertsSent > 0, "S1g no re-latch after a completed clear");
  }

  /* ===== S2 (J): forged RELEASE ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    p4.dropUp.insert("ESTOP-01");      /* real answers withheld */
    p4.challengeRound();
    const std::string nonce = lastNonce(p4);
    p4.deliver(forgedAuth("ESTOP-01", p4.episode, nonce.c_str()));
    const int s = p4.slotOf("ESTOP-01");
    expect(s >= 0 && p4.t[s].inputOpen && !p4.t[s].reportKnown,
           "S2a AUTH RELEASED tagged with the wrong key is rejected");
    std::string tampered = forgedAuth("ESTOP-01", p4.episode, nonce.c_str());
    tampered[tampered.size() - 1] = tampered.back() == '0' ? '1' : '0';
    p4.deliver(tampered);
    expect(p4.t[s].inputOpen && !p4.directorClear(), "S2b tampered tag rejected; clear refused");
    p4.deliver("ESTOP:RELEASE:ESTOP-01");
    p4.deliver("STATUS:NORMAL:IN=RELEASED:L=0:A=0:ESTOP-01");
    expect(p4.t[s].inputOpen && !p4.directorClear(),
           "S2c unauthenticated RELEASE / STATUS lines change nothing");
  }

  /* ===== S3 (J): forged ANNOUNCE ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    C3 fake("ESTOP-01", nullptr, false);
    fake.m.latched = 0; fake.m.input_open = 0;
    bool handled = false;
    const ShowduinoStationResult r = p4.deliver(fake.announce(), &handled);
    const int s = p4.slotOf("ESTOP-01");
    expect(handled && !r.accepted && r.slot < 0 && p4.t[s].inputOpen && !p4.t[s].online,
           "S3a forged ANNOUNCE IN=RELEASED L=0 is display-only: no state change");
    p4.dropUp.insert("ESTOP-01");
    expect(!p4.directorClear(), "S3b forged ANNOUNCE cannot satisfy clear");
    C3 stranger("ESTOP-77", nullptr, false);
    p4.deliver(stranger.announce());
    expect(p4.slotOf("ESTOP-77") < 0, "S3c ANNOUNCE never creates a station entry");
  }

  /* ===== S4 (J): replayed valid RELEASE ===== */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.challengeRound();
    const std::string captured = lastUp(p4, "AUTH:");
    a.pressed = true; p4.pump(a);
    const int s = p4.slotOf("ESTOP-01");
    p4.dropUp.insert("ESTOP-01");
    expect(s >= 0 && p4.t[s].inputOpen == 1, "S4 setup: press recorded");
    expect(!p4.deliver(captured).verified, "S4a replayed AUTH RELEASED (same nonce, old episode) rejected");
    expect(!p4.directorClear(), "S4b replay cannot satisfy clear");
    /* Replay within the same episode: the nonce is single use per station. */
    p4.dropUp.clear();
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    const std::string fresh = lastUp(p4, "AUTH:");
    a.pressed = true; p4.pump(a);
    p4.dropUp.insert("ESTOP-01");
    expect(!p4.deliver(fresh).verified, "S4c replayed AUTH from the same episode rejected (nonce used)");
    p4.advance(SHOWDUINO_AUTH_WINDOW_MS + 1);
    expect(!p4.deliver(fresh).verified && !p4.directorClear(), "S4d replay after the window rejected");
  }

  /* ===== S5 (J): replay following P4 reboot ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    const std::string capturedAuth = lastUp(p4, "AUTH:");
    const std::string capturedAck = lastDown(p4, ":ACK:");
    a.pressed = true; p4.pump(a);
    P4 after; rebootP4(p4, after);
    after.dropUp.insert("ESTOP-01");
    after.deliver(capturedAuth);
    const int s = after.slotOf("ESTOP-01");
    expect(s >= 0 && after.t[s].inputOpen && !after.t[s].reportKnown,
           "S5a pre-reboot AUTH RELEASED rejected after P4 restart (new challenges, new episode)");
    expect(after.episode > p4.episode, "S5b post-reboot episode id is above every pre-reboot value");
    expect(!after.directorClear(), "S5c clear refused after reboot without fresh proof");
    const uint32_t before = a.nvsCounter;
    after.down(&a, capturedAck);
    expect(a.lastCmdResult == SHOWDUINO_RESP_REJECT_REPLAY && a.nvsCounter == before,
           "S5d pre-reboot CMD replayed to the station is rejected by its counter");
  }

  /* ===== S6 (J): replay following C3 reboot ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    expect(p4.directorClear(), "S6a setup: clear accepted");
    const std::string gcl = lastDown(p4, ":GCL:");
    a.pressed = true; p4.pump(a);                 /* new episode */
    a.boot();                                     /* C3 power cycle: RAM lost, NVS kept */
    expect(a.st.lastCmdCounter == a.nvsCounter && a.nvsCounter > 0,
           "S6b C3 restores its last accepted counter from NVS");
    p4.dropDown.clear();
    p4.down(&a, gcl);
    expect(a.lastCmdResult == SHOWDUINO_RESP_REJECT_REPLAY && a.m.latched == 1,
           "S6c old GCL replayed after C3 reboot is rejected; station stays latched");
    p4.advance(300); p4.pump(a);
    expect(a.st.latchedEpisode == p4.episode, "S6d rebooted station relearns the episode from a fresh ACK");
  }

  /* ===== S7 (J): incorrect station identity ===== */
  {
    C3 a("ESTOP-01", &KA, true), b("ESTOP-02", &KB, false);
    P4 p4; p4.stations = {&a, &b}; p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB);
    p4.pump(a);
    p4.dropUp.insert("ESTOP-01");
    p4.challengeRound();
    const std::string nonce = lastNonce(p4);
    char buf[SHOWDUINO_NODE_COMMAND_MAX];
    showduino_auth_format_auth("ESTOP-01", p4.episode, nonce.c_str(), 0, 0, 0, KB.b,
                               SHOWDUINO_AUTH_KEY_BYTES, hmacSha256, buf, sizeof(buf));
    p4.deliver(buf);
    const int s = p4.slotOf("ESTOP-01");
    expect(p4.t[s].inputOpen && !p4.t[s].reportKnown,
           "S7a station B cannot report RELEASED for station A (A's key required)");
    p4.dropUp.clear();
    a.pressed = false; p4.pump(a);
    const std::string ackForA = lastDown(p4, "CMD:ESTOP-01:");
    b.m.latched = 1;
    p4.down(&b, ackForA);
    expect(b.lastCmdResult == SHOWDUINO_RESP_REJECT_IDENTITY, "S7b command for A is rejected by B");
    expect(p4.directorClear(), "S7c genuine proofs from both stations allow clear");
  }

  /* ===== S8 (J): expired freshness and episode mismatch ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.dropUp.insert("ESTOP-01");
    p4.challengeRound();
    const std::string held = lastUp(p4, "AUTH:");
    p4.advance(SHOWDUINO_AUTH_WINDOW_MS + 50);
    const int s = p4.slotOf("ESTOP-01");
    expect(!p4.deliver(held).verified && p4.t[s].reportKnown == 0, "S8a AUTH delivered after the freshness window is rejected");
    p4.dropUp.clear();
    p4.challengeRound();
    expect(p4.t[s].inputOpen == 0 && p4.t[s].online == 1, "S8b a fresh answer is accepted");
    p4.timeout("ESTOP-01");
    expect(p4.t[s].online == 0, "S8c timeout sets OFFLINE");
    /* Episode binding: an AUTH for the old episode is useless after clear. */
    expect(p4.directorClear(), "S8d clear accepted with fresh proof");
    a.pressed = true; p4.pump(a);
    p4.dropUp.insert("ESTOP-01");
    p4.challengeRound();
    p4.dropUp.clear();
    const std::string latchedAuth = lastUp(p4, "AUTH:");
    p4.advance(10);
    a.pressed = false; p4.pump(a);
    const bool first = p4.deliver(latchedAuth).verified;
    const bool second = p4.deliver(latchedAuth).verified;
    expect(!latchedAuth.empty() && first && !second,
           "S8e a withheld AUTH is accepted once inside the window, never twice");
  }

  /* ===== S9 (J): missing or corrupted credentials ===== */
  {
    C3 a("ESTOP-01", nullptr, true);          /* station never provisioned */
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    p4.challengeRound();
    expect(lastUp(p4, "AUTH:").empty(), "S9a unprovisioned station does not answer challenges");
    a.pressed = false; p4.pump(a);
    expect(!p4.directorClear() && p4.lastDecision != SHOWDUINO_CLEAR_OK,
           "S9b without a key the station cannot prove RELEASED: clear refused");
    C3 b("ESTOP-02", &KB, false);
    Key corrupt = KB; corrupt.b[5] ^= 0x40;
    P4 q; q.stations = {&b}; q.provision("ESTOP-02", corrupt);
    q.challengeRound();
    const int s = q.slotOf("ESTOP-02");
    expect(s < 0, "S9c corrupted P4 key: AUTH fails, nothing recorded");
    b.pressed = true; q.pump(b);
    b.pressed = false; q.pump(b);
    expect(!q.directorClear() && q.lastDecision != SHOWDUINO_CLEAR_OK,
           "S9d corrupted key keeps the emergency latched (fail closed)");
    P4 r; r.stations = {&b};             /* P4 has no key at all for a station that asserted */
    b.m.latched = 0; b.pressed = true; r.pump(b);
    b.pressed = false; r.pump(b);
    expect(r.globalLatched && !r.directorClear() && r.lastDecision == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S9e station unknown to the P4 key table: ASSERT latches and blocks clear");
  }

  /* ===== S10 (J): replaced station hardware ===== */
  {
    C3 oldBoard("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&oldBoard}; p4.provision("ESTOP-01", KA);
    p4.pump(oldBoard);
    const std::string oldAuth = (p4.challengeRound(), lastUp(p4, "AUTH:"));
    /* Board destroyed while latched. New board provisioned locally with a new key. */
    C3 newBoard("ESTOP-01", &KA2, false);
    p4.stations = {&newBoard};
    p4.provision("ESTOP-01", KA2);
    const int s = p4.slotOf("ESTOP-01");
    expect(p4.t[s].inputOpen == 1 && p4.globalLatched,
           "S10a re-keying while latched keeps the station blocking (no silent release)");
    p4.deliver(oldAuth);
    C3 oldAgain("ESTOP-01", &KA, false);
    p4.stations = {&oldAgain};
    p4.challengeRound();
    expect(p4.t[s].inputOpen == 1, "S10b the old board's key is no longer accepted");
    p4.stations = {&newBoard};
    p4.advance(10);
    expect(p4.directorClear() && newBoard.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "S10c the replacement proves RELEASED with its own key; clear accepted");
  }

  /* ===== S11 (J): interrupted persistence ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.failCounterWrites = 1000;
    p4.pump(a);
    expect(p4.globalLatched && p4.counterFault && p4.episode == 0 && a.m.acked == 0,
           "S11a counter block not durable: latch still engages, no command issued");
    a.pressed = false; p4.pump(a);
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_STORAGE,
           "S11b counter fault blocks clear (fail closed)");
    p4.failCounterWrites = 0;
    p4.advance(SHOWDUINO_EMERGENCY_HEARTBEAT_MS + 1); p4.pump(a);
    expect(!p4.counterFault && p4.episode != 0 && a.m.acked == 1,
           "S11b2 storage recovers: episode allocated, station ACKed");
    p4.advance(10);
    expect(p4.directorClear(), "S11b3 after recovery and fresh proof, clear accepted");
    C3 b("ESTOP-02", &KB, true);
    P4 q; q.stations = {&b}; q.provision("ESTOP-02", KB);
    b.failCounterWrites = 1;
    q.pump(b);
    expect(b.lastCmdResult == SHOWDUINO_RESP_REJECT_PERSIST && b.m.acked == 0 && b.nvsCounter == 0,
           "S11c station counter not durable: ACK refused, station keeps asserting");
    q.advance(SHOWDUINO_EMERGENCY_HEARTBEAT_MS + 1); q.pump(b);
    expect(b.m.acked == 1, "S11d next ACK after storage recovers is accepted");
    b.pressed = false; q.pump(b);
    q.store.failWrites = 1;
    q.deliver("ESTOP:ASSERT:ESTOP-03:X");
    expect(q.persistDirty && !q.directorClear(), "S11e blocker-set write failure blocks clear");
  }

  /* ===== S12 (J): mixed authenticated and legacy stations ===== */
  {
    C3 a("ESTOP-01", &KA, false), legacy("ESTOP-09", nullptr, true, true);
    P4 p4; p4.stations = {&a, &legacy}; p4.provision("ESTOP-01", KA);
    legacy.pressed = true; p4.pump(legacy);
    expect(p4.globalLatched, "S12a legacy ASSERT still latches immediately");
    legacy.pressed = false; p4.pump(legacy);
    p4.deliver(legacy.announce());
    const int s = p4.slotOf("ESTOP-09");
    expect(s >= 0 && p4.t[s].inputOpen && !p4.t[s].reportKnown,
           "S12b legacy ANNOUNCE RELEASED does not verify the legacy station");
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S12c an asserted legacy station blocks clear (no insecure fallback)");
    expect(showduino_station_model_dismiss(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, "ESTOP-09", 0) == 1,
           "S12d local maintenance dismiss removes the unprovisioned entry");
    expect(showduino_station_model_dismiss(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, "ESTOP-01", 1) == 0,
           "S12e dismiss refuses a provisioned station");
    p4.persist();
    expect(p4.directorClear(), "S12f after maintenance, the provisioned station's proof allows clear");
    expect(legacy.m.latched == 1 && legacy.rejectedUnauth == 0,
           "S12g P4 sends no unauthenticated clear to the legacy station");
  }

  /* ===== S13 (J): multiple simultaneous stations, one silenced by the relay ===== */
  {
    C3 a("ESTOP-01", &KA, false), b("ESTOP-02", &KB, false), c("ESTOP-03", &KC, false);
    P4 p4; p4.stations = {&a, &b, &c};
    p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB); p4.provision("ESTOP-03", KC);
    a.pressed = b.pressed = true; p4.pumpAll();
    expect(p4.globalTriggers == 1 && a.m.acked && b.m.acked, "S13a two presses, one episode, both ACKed");
    expect(!p4.directorClear(), "S13b both pressed: refused");
    a.pressed = false; p4.pumpAll();
    expect(!p4.directorClear(), "S13c one still pressed: refused");
    b.pressed = false; p4.pumpAll();
    p4.dropUp.insert("ESTOP-03");
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_PROOF,
           "S13d relay drops station C (never asserted): its missing proof blocks clear");
    p4.dropUp.clear();
    p4.advance(10);
    expect(p4.directorClear() && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL &&
           b.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "S13e all three prove RELEASED: clear accepted and both pressed stations normalise");
  }

  /* ===== S14 (J): legitimate clearance following authenticated recovery after P4 reboot ===== */
  {
    C3 a("ESTOP-01", &KA, true), b("ESTOP-02", &KB, false);
    P4 p4; p4.stations = {&a, &b}; p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB);
    p4.pump(a);
    expect(p4.store.value.find("ESTOP-01") != std::string::npos, "S14a asserted station stored");
    P4 after; rebootP4(p4, after);
    const int s = after.slotOf("ESTOP-01");
    expect(s >= 0 && after.t[s].inputOpen && after.t[s].asserted, "S14b reboot restores the blocker");
    after.ackSweep();
    expect(a.st.latchedEpisode == after.episode, "S14c ACK sweep teaches the station the new episode");
    expect(!after.directorClear(), "S14d still PRESSED after reboot: refused");
    a.pressed = false; after.pump(a);
    after.advance(10);
    expect(after.directorClear() && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "S14e authenticated recovery: clear accepted and GCL for the new episode applies");
  }

  /* ===== S15 (J): press after a verified release, ASSERT dropped by the relay ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    const int s = p4.slotOf("ESTOP-01");
    expect(p4.t[s].inputOpen == 0 && p4.t[s].online, "S15a station verified RELEASED and online");
    p4.dropUp.insert("ESTOP-01");
    a.pressed = true; p4.pump(a);
    p4.dropUp.clear();
    p4.advance(100);
    expect(!p4.directorClear() && p4.t[s].inputOpen == 1,
           "S15b the confirm-time proof shows PRESSED even though the ASSERT was dropped");
    p4.dropUp.insert("ESTOP-01");
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S15c relay withholding the proof: clear refused");
  }

  /* ===== S16 (J): unauthenticated ACK / GLOBAL:OBSERVED / EMERGENCY:CLEAR to the station ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    a.pressed = false;
    showduino_emergency_machine_input(&a.m, 0);
    p4.down(&a, "ESTOP:GLOBAL:OBSERVED");
    p4.down(&a, "EMERGENCY:CLEAR");
    p4.down(&a, "ESTOP:ACK:LATCHED");
    expect(a.m.latched == 1 && a.m.acked == 0 && a.rejectedUnauth == 3,
           "S16a station ignores unauthenticated ACK, GLOBAL:OBSERVED and EMERGENCY:CLEAR");
    char gcl[SHOWDUINO_NODE_COMMAND_MAX];
    showduino_auth_format_cmd("ESTOP-01", 999, "GCL", 5, KB.b, SHOWDUINO_AUTH_KEY_BYTES,
                              hmacSha256, gcl, sizeof(gcl));
    p4.down(&a, gcl);
    expect(a.lastCmdResult == SHOWDUINO_RESP_REJECT_TAG && a.m.latched == 1,
           "S16b GCL forged with another key is rejected");
    C3 idle("ESTOP-03", &KC, false);
    char ack[SHOWDUINO_NODE_COMMAND_MAX];
    showduino_auth_format_cmd("ESTOP-03", 40, "ACK", 7, KC.b, SHOWDUINO_AUTH_KEY_BYTES,
                              hmacSha256, ack, sizeof(ack));
    idle.receive(ack);
    expect(idle.lastCmdResult == SHOWDUINO_RESP_REJECT_STATE && idle.nvsCounter == 0 &&
           idle.m.acked == 0 && idle.st.latchedEpisode == 0,
           "S16c a valid ACK to a NORMAL station is refused: no flash write, no state");
  }

  /* ===== S17 (J): GCL held back by the relay, delivered in a later episode ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.dropDownMatch = ":GCL:";
    p4.advance(10);
    expect(p4.directorClear(), "S17a clear accepted; relay withholds the GCL");
    const std::string held = lastDown(p4, ":GCL:");
    p4.dropDownMatch.clear();
    /* Station is still latched locally; its next verified AUTH shows L=1 and re-latches. */
    p4.advance(1000); p4.challengeRound();
    expect(p4.globalLatched && p4.globalTriggers == 2,
           "S17b station that missed the GCL re-latches the P4 (fail safe)");
    expect(a.st.latchedEpisode == p4.episode, "S17c it learns the new episode from a fresh ACK");
    p4.down(&a, held);
    expect(a.m.latched == 1 && a.lastCmdResult < SHOWDUINO_RESP_ACK,
           "S17d the held GCL from the old episode is rejected");
    p4.advance(10);
    expect(p4.directorClear() && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "S17e the new episode clears normally");
  }

  /* ===== S17f (J): held GCL meets a station that latched afresh ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.dropDownMatch = ":GCL:";
    p4.advance(10);
    expect(p4.directorClear(), "S17f-a clear accepted; relay withholds the GCL");
    const std::string held = lastDown(p4, ":GCL:");
    a.localUsbObserve();                       /* technician resets the station locally */
    expect(a.m.latched == 0, "S17f-b station NORMAL by local USB maintenance");
    p4.dropDownMatch = ":ACK:";                /* relay now withholds the new ACK */
    a.pressed = true; p4.advance(10); p4.pump(a);
    a.pressed = false; p4.pump(a);
    expect(p4.globalLatched && a.m.latched == 1 && a.st.latchedEpisode == 0,
           "S17f-c fresh latch in a new episode; station has not learned it");
    p4.dropDownMatch.clear();
    p4.down(&a, held);
    expect(a.lastCmdResult == SHOWDUINO_RESP_REJECT_STATE && a.m.latched == 1,
           "S17f-d GCL from the old episode cannot normalise a freshly latched station");
  }

  /* ===== S25 (J): request-time gate and an offline verified station ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    p4.timeout("ESTOP-01");
    const int d = showduino_station_clear_decision(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, p4.ids,
                                                   0, 0, 0, 0, 0, 0, 1);
    expect(d == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S25a CLEAR_REQUEST gate: a verified RELEASED station that went OFFLINE blocks");
  }

  /* ===== S26 (J): revoked station that asserted keeps blocking until dismissed ===== */
  {
    C3 a("ESTOP-01", &KA, true), b("ESTOP-02", &KB, false);
    P4 p4; p4.stations = {&a, &b}; p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    p4.revoke("ESTOP-01");                    /* key withdrawn on the P4 */
    p4.timeout("ESTOP-01");                   /* its answers no longer verify */
    p4.advance(10);
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_STATION,
           "S26a revoking a key does not release an asserted station (it is now unverifiable)");
    expect(showduino_station_model_dismiss(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, "ESTOP-01",
                                           p4.isProvisioned("ESTOP-01") ? 1 : 0) == 1,
           "S26b after revoke, local maintenance can dismiss it explicitly");
    p4.persist(); p4.advance(10);
    expect(p4.directorClear(), "S26c remaining provisioned station proves RELEASED: clear accepted");
  }

  /* ===== S27 (J): a provisioned station that never communicates ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a};
    p4.provision("ESTOP-01", KA); p4.provision("ESTOP-05", KC);   /* ESTOP-05 is silent */
    p4.pump(a);
    a.pressed = false; p4.pump(a); p4.advance(10);
    expect(p4.slotOf("ESTOP-05") < 0 && !p4.directorClear() &&
           p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_PROOF,
           "S27a a registered station that is absent or silenced blocks clear (fail closed)");
  }

  /* ===== S18 (J): table filled by forged ASSERT IDs ===== */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    for (int i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      char line[48];
      std::snprintf(line, sizeof(line), "ESTOP:ASSERT:ESTOP-%02d:F", 50 + i);
      p4.deliver(line);
    }
    p4.globalLatched = false;           /* model an operator clear of the first latch */
    a.pressed = true; p4.pump(a);
    expect(p4.globalLatched && p4.tableOverflow,
           "S18a real ASSERT still latches when the table is full of forged IDs");
    const uint32_t t0 = p4.now;
    expect(!p4.directorClear() && p4.lastDecision == SHOWDUINO_CLEAR_BLOCK_OVERFLOW &&
           p4.lastTxnResult == SHOWDUINO_TXN_REJECT_BLOCKED && p4.now == t0,
           "S18b table overflow blocks clear immediately (no verification wait)");
  }

  /* ===== S19 (001I carried): bare ASSERT never restores freshness ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a);
    p4.challengeRound();
    const int s = p4.slotOf("ESTOP-01");
    p4.timeout("ESTOP-01");
    p4.deliver("ESTOP:ASSERT:ESTOP-01:TEST");
    expect(p4.t[s].online == 0, "S19a ASSERT after timeout does not restore ONLINE");
    p4.dropUp.insert("ESTOP-01");
    expect(!p4.directorClear(), "S19b clear refused without a new verified answer");
  }

  /* ===== S20 (001I carried): reboot with several stations in different states ===== */
  {
    C3 a("ESTOP-01", &KA, true), b("ESTOP-02", &KB, true), c("ESTOP-03", &KC, false);
    P4 p4; p4.stations = {&a, &b, &c};
    p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB); p4.provision("ESTOP-03", KC);
    p4.pumpAll();
    a.pressed = false; p4.pumpAll(); p4.challengeRound();
    P4 after; rebootP4(p4, after);
    expect(after.blocking() == 2 && after.slotOf("ESTOP-03") < 0,
           "S20a both asserted stations restored as blockers; idle C not stored");
    after.ackSweep(); after.advance(10);
    expect(!after.directorClear(), "S20b B still pressed: refused");
    b.pressed = false; after.pumpAll(); after.advance(10);
    expect(after.directorClear(), "S20c all prove RELEASED: clear accepted");
  }

  /* ===== S21 (001I carried): unreadable stored record after a latched reboot ===== */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.store.value = "B1;garbage#0000"; p4.store.valid = true;
    P4 after; rebootP4(p4, after);
    after.challengeRound();
    expect(after.bootUnknown && !after.directorClear() &&
           after.lastDecision == SHOWDUINO_CLEAR_BLOCK_STORAGE,
           "S21a corrupt blocker record keeps clear refused even with fresh proofs");
  }

  /* ===== S22 (001I carried): accepted clear ends the episode in storage ===== */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    p4.pump(a);
    a.pressed = false; p4.pump(a); p4.advance(10);
    expect(p4.directorClear() && p4.store.value.find("ESTOP-01") == std::string::npos,
           "S22a accepted clear stores an empty blocker set");
    P4 after; rebootP4(p4, after, false);
    expect(after.blocking() == 0, "S22b reboot after clear restores nothing");
  }

  /* ===== S23 (001I carried): stored set is never dropped silently ===== */
  {
    ShowduinoStationModel t[3]{};
    for (int i = 0; i < 3; ++i) {
      char id[16]; std::snprintf(id, sizeof(id), "ESTOP-%02d", i + 1);
      showduino_station_model_restore(&t[i], id, "");
    }
    ShowduinoBlocker out[SHOWDUINO_BLOCKER_MAX];
    int n = 0;
    expect(showduino_station_model_collect_required(t, 3, out, 1, &n) == 0,
           "S23a overflow of the stored set reports failure");
  }

  /* ======================= MISSION 001K: Director authority ======================= */
  const Key KD = makeKey(40), KD2 = makeKey(41), KDX = makeKey(42);

  /* K1: genuine Director request and confirmation */
  {
    C3 a("ESTOP-01", &KA, true);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode();
    expect(d.tapClear() && p4.lastDirResult == SHOWDUINO_DIR_OK && p4.txn.state == SHOWDUINO_TXN_OPEN &&
           d.s.txnHex[0], "K1a signed REQ accepted; P4 opens a transaction and tells the Director");
    p4.advance(10);
    expect(d.tapConfirm() && p4.clearsAccepted == 1 && !p4.globalLatched &&
           a.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "K1b signed CFM + fresh station proofs: clear accepted, station normalised");
  }

  /* K2/K3/K22: forged request, forged confirmation, failed auth in an otherwise valid clear */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    Director fake("DIR-01", KDX); fake.p4 = &p4;  /* attacker without the key */
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); fake.s.episode = p4.episode;
    fake.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_TAG && p4.txn.state == SHOWDUINO_TXN_IDLE,
           "K2 forged REQ rejected; no transaction opened");
    d.tapClear();
    std::snprintf(fake.s.txnHex, sizeof(fake.s.txnHex), "%s", p4.txn.nonceHex);
    fake.s.txnEpisode = p4.episode;
    fake.tapConfirm();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_TAG && p4.globalLatched &&
           p4.txn.state == SHOWDUINO_TXN_OPEN,
           "K3/K22 forged CFM rejected with every station released; latch kept, real txn still open");
    p4.advance(10);
    expect(d.tapConfirm() && !p4.globalLatched, "K3b the genuine confirm still works afterwards");
  }

  /* K4: S3 tampers with the episode notice -> Director signs the wrong episode */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    d.onNotice("STATE:EMERGENCY:EPISODE:7fffffff");   /* forged by the relay */
    d.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_EPISODE && p4.txn.state == SHOWDUINO_TXN_IDLE,
           "K4 Director message for an episode the P4 is not in is rejected (fail closed)");
  }

  /* K5/K9/K23: captured Director traffic replayed by a compromised S3 */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10); d.tapConfirm();
    expect(!p4.globalLatched, "K5 setup: genuine clear in episode 1");
    const std::string oldReq = d.sent[0], oldCfm = d.sent[1];
    p4.fromComms(oldCfm);
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_EPISODE, "K23 replayed CFM after the clear rejected");
    a.pressed = true; p4.advance(100); p4.pump(a); a.pressed = false; p4.pump(a);
    expect(p4.globalLatched, "K5 new emergency (episode 2)");
    p4.fromComms(oldReq);
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_EPISODE && p4.txn.state == SHOWDUINO_TXN_IDLE,
           "K5a replayed REQ from episode 1 rejected in episode 2");
    p4.fromComms(oldCfm);
    expect(p4.lastDirResult != SHOWDUINO_DIR_OK && p4.globalLatched,
           "K5b replayed CFM from episode 1 rejected; latch kept");
    p4.publishEpisode(); d.tapClear();
    const std::string req2 = d.sent.back();
    p4.fromComms(req2);
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_REPLAY, "K9 same-episode REQ replay rejected by counter");
  }

  /* K6/K7: replay after Director restart and after P4 restart */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear();
    const std::string req = d.sent.back();
    d.reboot();
    p4.fromComms(req);
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_REPLAY, "K6a pre-restart REQ replay rejected");
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    expect(p4.lastDirResult == SHOWDUINO_DIR_OK && p4.txn.state == SHOWDUINO_TXN_OPEN,
           "K6b restarted Director signs fresh requests (counter resumed above stored block)");
    const std::string openNonce = p4.txn.nonceHex;
    P4 after; rebootP4(p4, after);
    after.fromComms(req);
    expect(after.lastDirResult == SHOWDUINO_DIR_REJECT_EPISODE || after.lastDirResult == SHOWDUINO_DIR_REJECT_REPLAY,
           "K7a pre-reboot REQ rejected after P4 restart");
    d.tapConfirm();   /* CFM for the transaction that died with the reboot */
    expect(after.lastDirResult != SHOWDUINO_DIR_OK && after.globalLatched,
           "K7b CFM for a pre-reboot transaction rejected; latch restored and kept");
    after.publishEpisode(); after.ackSweep(); d.tapClear(); after.advance(10); d.tapConfirm();
    expect(!after.globalLatched && std::string(openNonce) != "",
           "K7c a fresh request/confirm after the P4 restart clears normally");
  }

  /* K8: S3-generated clearance (plain text) */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    for (const char *c : {"EMERGENCY:CLEAR", "EMERGENCY:CLEAR_REQUEST", "EMERGENCY:CLEAR_CONFIRM"}) {
      p4.fromComms(c);
    }
    expect(p4.globalLatched && p4.txn.state == SHOWDUINO_TXN_IDLE &&
           p4.lastReply == "EMERGENCY:CLEAR:REJECTED:AUTH_REQUIRED",
           "K8 plain clear request/confirm from the UART never clears (AUTH_REQUIRED)");
  }

  /* K10: missing Director -> only the USB maintenance authority can clear */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);   /* no Director enrolled */
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    Director rogue("DIR-01", KD); rogue.p4 = &p4; rogue.s.episode = p4.episode;
    rogue.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_NO_KEY && p4.globalLatched,
           "K10a with no enrolled Director nothing on the UART can clear");
    p4.advance(10);
    expect(p4.localClear(), "K10b USB maintenance clear (with station proofs) works");
  }

  /* K11: missing Comms Controller */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.dropAllUp = true;                       /* S3 dead: no station answers */
    expect(!p4.localClear() && p4.lastTxnResult == SHOWDUINO_TXN_REJECT_UNVERIFIED &&
           p4.lastDecision != SHOWDUINO_CLEAR_OK,
           "K11a no relay: even the USB clear is refused while provisioned stations cannot prove RELEASED");
    p4.revoke("ESTOP-01");
    showduino_station_model_dismiss(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, "ESTOP-01", 0);
    p4.persist(); p4.advance(10);
    expect(p4.localClear(), "K11b documented maintenance (inspect, revoke, dismiss) then USB clear");
  }

  /* K12: Director replacement */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director oldD("DIR-01", KD); oldD.p4 = &p4; p4.director = &oldD;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.enrollDirector("DIR-01", KD2);            /* replacement enrolled during the emergency */
    Director newD("DIR-01", KD2); newD.p4 = &p4; p4.director = &newD;
    p4.publishEpisode(); newD.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_RESTRICTED && p4.globalLatched,
           "K12a a Director enrolled during an active emergency cannot clear it");
    oldD.s.episode = p4.episode; oldD.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_TAG, "K12b the replaced Director's key no longer works");
    p4.advance(10);
    expect(p4.localClear() && p4.dirRec.restricted == 0,
           "K12c USB maintenance clear ends the episode and lifts the restriction");
    a.pressed = true; p4.advance(100); p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); newD.tapClear(); p4.advance(10); newD.tapConfirm();
    expect(!p4.globalLatched, "K12d the new Director clears the next emergency normally");
  }

  /* K13: wrong or corrupted Director key on either side */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA);
    Key bad = KD; bad.b[3] ^= 0x10;
    p4.enrollDirector("DIR-01", bad);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_TAG && p4.globalLatched,
           "K13 corrupted key: no request accepted, latch kept");
  }

  /* K14: stale station proof — answered before the confirm, then the relay hides it */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.challengeRound();                                   /* RELEASED proof before the request */
    p4.publishEpisode(); d.tapClear(); p4.challengeRound(); /* and after the request */
    p4.dropAllUp = true;
    p4.advance(10); d.tapConfirm();
    expect(p4.globalLatched && p4.lastTxnResult == SHOWDUINO_TXN_REJECT_UNVERIFIED,
           "K14 proofs from before the confirm never count; no fresh answer -> refused");
  }

  /* K15: station press between request and confirm */
  {
    C3 a("ESTOP-01", &KA, false), b("ESTOP-02", &KB, false);
    P4 p4; p4.stations = {&a, &b}; p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB);
    p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear();
    b.pressed = true; p4.advance(50); p4.pump(b);         /* operator sees nothing yet */
    d.tapConfirm();
    expect(p4.globalLatched && p4.lastDirResult == SHOWDUINO_DIR_REJECT_TXN,
           "K15 an ASSERT between request and confirm voids the transaction");
  }

  /* K16a: press after a valid proof, honest relay */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear();
    p4.holdUp = true;                  /* answers delayed in the relay */
    p4.advance(10); d.tapConfirm();
    expect(p4.globalLatched || p4.lastTxnResult != SHOWDUINO_TXN_ACCEPT,
           "K16a setup: verification pending while answers are in flight");
  }
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    /* Press during verification with an honest relay: the ASSERT reaches the P4. */
    p4.holdUp = true;
    std::string cfm;
    {
      char buf[128];
      showduino_dir_sign_cfm(&d.s, hmacSha256, Director::persistCb, &d, buf, sizeof(buf));
      cfm = buf;
    }
    p4.holdUp = true;
    /* Start verification by hand so the press can land in the middle. */
    p4.lastDirResult = showduino_dir_accept(&p4.dirRec, cfm.c_str(), p4.episode,
                                            showduino_clear_txn_open_nonce(&p4.txn), hmacSha256,
                                            P4::dirPersistCb, &p4, nullptr);
    showduino_clear_txn_begin_verify(&p4.txn, p4.episode, p4.now, showduino_auth_chal_next_seq(&p4.chal));
    p4.challengeRound();                                   /* RELEASED answers held */
    /* Station is between heartbeats with its assert burst finished: only a new
     * press edge can make it ASSERT now. */
    a.m.pending_assert = 0; a.m.burst_left = 0; a.m.next_tx_ms = p4.now + 1500;
    const int assertsBefore = a.assertsSent;
    a.pressed = true; p4.pump(a);                          /* ASSERT delivered */
    expect(a.assertsSent == assertsBefore + 1,
           "K16b0 a re-press while still latched asserts at once (not at the next heartbeat)");
    p4.holdUp = false; p4.releaseHeld();
    const int r = showduino_clear_txn_evaluate(&p4.txn, p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, p4.ids,
                                               p4.keyCount, p4.episode, p4.now, 0, 0, 0, 0, 1);
    expect(p4.txnSuperseded && r != SHOWDUINO_TXN_ACCEPT,
           "K16b press after a valid RELEASED proof, before the clear: ASSERT aborts verification");
  }

  /* K16c BOUNDARY: hostile S3 suppresses the ASSERT and delays the earlier RELEASED answer */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    char buf[128];
    showduino_dir_sign_cfm(&d.s, hmacSha256, Director::persistCb, &d, buf, sizeof(buf));
    p4.lastDirResult = showduino_dir_accept(&p4.dirRec, buf, p4.episode,
                                            showduino_clear_txn_open_nonce(&p4.txn), hmacSha256,
                                            P4::dirPersistCb, &p4, nullptr);
    showduino_clear_txn_begin_verify(&p4.txn, p4.episode, p4.now, showduino_auth_chal_next_seq(&p4.chal));
    p4.holdUp = true;
    p4.challengeRound();                                   /* genuine post-confirm RELEASED, held */
    p4.dropAssertUp = true; p4.dropUp.insert("ESTOP-01");  /* hostile relay hides the press */
    a.pressed = true; p4.pump(a);
    p4.dropUp.clear(); p4.holdUp = false;
    p4.advance(300); p4.releaseHeld();                     /* still inside the 1000 ms window */
    const int r = showduino_clear_txn_evaluate(&p4.txn, p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, p4.ids,
                                               p4.keyCount, p4.episode, p4.now, 0, 0, 0, 0, 1);
    expect(r == SHOWDUINO_TXN_ACCEPT && a.m.input_open == 1,
           "K16c BOUNDARY: a hostile S3 that hides a press made after the station's post-confirm "
           "RELEASED answer can still get a clear accepted (single wireless path; J-14 residual)");
    p4.advance(SHOWDUINO_CLEAR_VERIFY_WINDOW_MS + 1);
    p4.holdUp = true; p4.dropAssertUp = false;
    expect(SHOWDUINO_CLEAR_VERIFY_WINDOW_MS <= 1000u,
           "K16d BOUNDARY: that exposure is limited to the verification window (<= 1000 ms)");
  }

  /* K17: lost then delayed station messages */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    p4.dropDownMatch = "CHAL:";                   /* first challenge lost */
    char buf[128];
    showduino_dir_sign_cfm(&d.s, hmacSha256, Director::persistCb, &d, buf, sizeof(buf));
    p4.lastDirResult = showduino_dir_accept(&p4.dirRec, buf, p4.episode,
                                            showduino_clear_txn_open_nonce(&p4.txn), hmacSha256,
                                            P4::dirPersistCb, &p4, nullptr);
    showduino_clear_txn_begin_verify(&p4.txn, p4.episode, p4.now, showduino_auth_chal_next_seq(&p4.chal));
    p4.challengeRound();
    p4.dropDownMatch.clear();
    p4.advance(SHOWDUINO_CLEAR_VERIFY_RECHAL_MS);
    if (showduino_clear_txn_want_rechallenge(&p4.txn, p4.now)) p4.challengeRound();
    const int r = showduino_clear_txn_evaluate(&p4.txn, p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, p4.ids,
                                               p4.keyCount, p4.episode, p4.now, 0, 0, 0, 0, 1);
    expect(r == SHOWDUINO_TXN_ACCEPT, "K17a a lost challenge is recovered by the re-challenge inside the window");
  }
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    p4.holdUp = true;            /* every answer delayed past the window */
    d.tapConfirm();
    expect(p4.globalLatched && p4.lastTxnResult == SHOWDUINO_TXN_REJECT_UNVERIFIED,
           "K17b answers delayed beyond the window: refused (fail closed)");
  }

  /* K18: multiple stations, one pressed */
  {
    C3 a("ESTOP-01", &KA, false), b("ESTOP-02", &KB, false), c("ESTOP-03", &KC, false);
    P4 p4; p4.stations = {&a, &b, &c};
    p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB); p4.provision("ESTOP-03", KC);
    p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = b.pressed = true; p4.pumpAll(); a.pressed = false; p4.pumpAll();
    p4.publishEpisode(); d.tapClear(); p4.advance(10); d.tapConfirm();
    expect(p4.globalLatched && p4.lastTxnResult == SHOWDUINO_TXN_REJECT_PRESSED &&
           std::string(p4.txn.blockId) == "ESTOP-02",
           "K18 one of three stations still pressed: refused and the station is named");
  }

  /* K19: physical P4 emergency input active */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear(); p4.advance(10);
    p4.physicalPressed = true;
    d.tapConfirm();
    expect(p4.globalLatched && p4.lastReply == "BUTTON_ACTIVE",
           "K19a GPIO25 loop open: signed confirm refused");
    p4.physicalPressed = false;
    p4.advance(10);
    expect(p4.localClear() && !p4.globalLatched, "K19b loop closed again: the USB clear proceeds");
  }

  /* K20: interrupted credential persistence */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode();
    p4.failDirWrites = 1;
    d.tapClear();
    expect(p4.lastDirResult == SHOWDUINO_DIR_REJECT_PERSIST && p4.txn.state == SHOWDUINO_TXN_IDLE &&
           p4.dirRec.lastCounter == 0, "K20a P4 cannot store the Director counter: REQ refused");
    d.failWrites = 1000;
    d.s.ctr.limit = d.s.ctr.next;  /* force a block write */
    expect(!d.tapClear() && d.signFailures == 1,
           "K20b Director cannot store its counter block: it signs nothing");
  }

  /* K21: invalid or unexpected command source */
  {
    expect(showduino_clear_entry(1, "DCLR:REQ:DIR-01:00000001:0000000000000000:00000001:x") ==
               SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE,
           "K21 a Director message on the P4 USB console is refused (wrong source)");
  }

  /* K25: realistic operator timing — stations keep running between request and confirm */
  {
    C3 a("ESTOP-01", &KA, false), b("ESTOP-02", &KB, false);
    P4 p4; p4.stations = {&a, &b}; p4.provision("ESTOP-01", KA); p4.provision("ESTOP-02", KB);
    p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pumpAll(); a.pressed = false;
    p4.runFor(2000);                               /* press burst (6 x 250 ms) completes */
    p4.publishEpisode(); d.tapClear();
    p4.runFor(6000);                               /* operator reads the dialog for ~6 s */
    expect(p4.txn.state == SHOWDUINO_TXN_OPEN,
           "K25a a latched, ACKed, released station's heartbeat does not cancel the open request");
    d.tapConfirm();
    expect(!p4.globalLatched && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL,
           "K25b confirm after a realistic delay clears and normalises the station");
  }
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pumpAll(); a.pressed = false;
    p4.runFor(2000);
    p4.publishEpisode(); d.tapClear();
    p4.runFor(1000);
    a.pressed = true;
    p4.runFor(100);                                /* pressed again while the dialog is open */
    expect(p4.txn.state == SHOWDUINO_TXN_IDLE,
           "K25c a new press while the request is open cancels it within one service tick");
  }

  /* K24: Director reboot or disconnect never releases */
  {
    C3 a("ESTOP-01", &KA, false);
    P4 p4; p4.stations = {&a}; p4.provision("ESTOP-01", KA); p4.enrollDirector("DIR-01", KD);
    Director d("DIR-01", KD); d.p4 = &p4; p4.director = &d;
    a.pressed = true; p4.pump(a); a.pressed = false; p4.pump(a);
    p4.publishEpisode(); d.tapClear();
    d.reboot(); p4.director = nullptr;
    p4.advance(SHOWDUINO_CLEAR_TXN_OPEN_MS + 1);
    const int r = showduino_clear_txn_evaluate(&p4.txn, p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, p4.ids,
                                               p4.keyCount, p4.episode, p4.now, 0, 0, 0, 0, 1);
    expect(p4.globalLatched && r == SHOWDUINO_TXN_REJECT_EXPIRED,
           "K24 Director gone mid-request: the request expires, the latch stays");
  }

  /* ===== S24 (J): proof window arithmetic ===== */
  {
    expect(showduino_station_proof_since(0, 10) == 9, "S24a no request: one of the two latest challenges");
    expect(showduino_station_proof_since(10, 12) == 11, "S24b request long ago: still the two latest");
    expect(showduino_station_proof_since(12, 12) == 12, "S24c only challenges after the request count");
    expect(showduino_station_proof_since(5, 1) == 5 && showduino_station_proof_since(0, 0) == 1,
           "S24d before any challenge nothing qualifies");
    expect(showduino_station_proof_since(0xFFFFFFFFu, 0xFFFFFFFFu) == 0xFFFFFFFFu,
           "S24e sequence arithmetic is wrap-safe");
  }

  std::printf("\n%d passed, %d failed\n", gPass, gFails);
  std::printf(gFails ? "FAILURES\n" : "ALL PASS\n");
  return gFails ? 1 : 0;
}
