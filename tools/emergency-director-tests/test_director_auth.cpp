/*
 * MISSION 001K host tests: Director operator-clear authentication, clearance
 * transaction and clear entry-point policy.
 *
 * Exercises protocol/showduino_director_auth.h and protocol/showduino_clear_txn.h,
 * the same functions the P4 and Director firmware call. HMAC-SHA256 from OpenSSL
 * (mbedtls on the ESP32). Build:
 *   g++ -std=c++17 -Wall -Wextra -Werror -Iprotocol test_director_auth.cpp -lcrypto
 */
#include "showduino_director_auth.h"
#include "showduino_clear_txn.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cstdio>
#include <cstring>
#include <string>

static int gFails = 0, gPass = 0;
static void expect(bool ok, const char *msg) {
  if (ok) { std::printf("PASS  %s\n", msg); gPass++; }
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

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

static uint64_t gRng = 0xC0FFEE1234567ull;
static int testRand(uint8_t *out, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    gRng = gRng * 6364136223846793005ull + 1442695040888963407ull;
    out[i] = (uint8_t)(gRng >> 33);
  }
  return 0;
}

struct Store { uint32_t v = 0; int fail = 0; };
static int persistCb(void *ctx, uint32_t v) {
  Store *s = static_cast<Store *>(ctx);
  if (s->fail > 0) { s->fail--; return 0; }
  s->v = v;
  return 1;
}

static void keyFill(uint8_t *k, uint8_t seed) {
  for (int i = 0; i < 32; ++i) k[i] = (uint8_t)(seed * 17 + i * 3 + 1);
}

static ShowduinoDirKeyRec makeRec(const char *id, uint8_t seed) {
  ShowduinoDirKeyRec r{};
  r.present = 1;
  std::snprintf(r.id, sizeof(r.id), "%s", id);
  keyFill(r.key, seed);
  return r;
}

static ShowduinoDirSigner makeSigner(const char *id, uint8_t seed, uint32_t ctrLimit = 1) {
  ShowduinoDirSigner s{};
  s.provisioned = 1;
  std::snprintf(s.id, sizeof(s.id), "%s", id);
  keyFill(s.key, seed);
  showduino_auth_counter_boot(&s.ctr, ctrLimit);
  return s;
}

int main() {
  /* ===== D0: wire format and identity ===== */
  {
    expect(showduino_dir_id_ok("DIR-01") && showduino_dir_id_ok("DIR-9999") &&
           !showduino_dir_id_ok("DIR-") && !showduino_dir_id_ok("ESTOP-01") &&
           !showduino_dir_id_ok("DIR-12345") && !showduino_dir_id_ok("DIR-1a"),
           "D0a Director ID format DIR-<1..4 digits>");
    uint8_t k[32]; keyFill(k, 1);
    char line[128];
    expect(showduino_dir_format(SHOWDUINO_DIR_OP_CFM, "DIR-9999", 0xFFFFFFFFu, "ffffffffffffffff",
                                0xFFFFFFFFu, k, 32, hmacSha256, line, sizeof(line)) &&
           std::strlen(line) <= SHOWDUINO_DIR_LINE_MAX,
           "D0b longest DCLR line fits the 95-character desk packet");
    expect(!showduino_dir_format(SHOWDUINO_DIR_OP_REQ, "DIR-01", 0, SHOWDUINO_DIR_TXN_ZERO, 1, k, 32,
                                 hmacSha256, line, sizeof(line)),
           "D0c no message can be signed for episode 0");
    ShowduinoDirMsg m;
    showduino_dir_format(SHOWDUINO_DIR_OP_REQ, "DIR-01", 5, SHOWDUINO_DIR_TXN_ZERO, 7, k, 32, hmacSha256,
                         line, sizeof(line));
    expect(showduino_dir_parse(line, &m) && m.op == SHOWDUINO_DIR_OP_REQ && m.episode == 5 &&
           m.counter == 7 && !std::strcmp(m.id, "DIR-01"), "D0d REQ round-trips");
    std::string bad = line;
    bad.replace(bad.find(":0000000000000000:"), 18, ":0000000000000001:");
    expect(!showduino_dir_parse(bad.c_str(), &m), "D0e REQ with a non-zero txn field is malformed");
    expect(!showduino_dir_parse("DCLR:CFM:DIR-01:00000005:zzzzzzzzzzzzzzzz:00000007:00", &m) &&
           !showduino_dir_parse("DCLR:XXX:DIR-01:", &m), "D0f malformed lines rejected");
  }

  /* ===== D1: genuine request and confirm ===== */
  ShowduinoDirKeyRec rec = makeRec("DIR-01", 1);
  ShowduinoDirSigner dir = makeSigner("DIR-01", 1);
  Store p4ctr, dirctr;
  {
    dir.episode = 9;
    char req[128], cfm[128];
    expect(showduino_dir_sign_req(&dir, hmacSha256, persistCb, &dirctr, req, sizeof(req)),
           "D1a Director signs a REQ for the reported episode");
    ShowduinoDirMsg m;
    expect(showduino_dir_accept(&rec, req, 9, nullptr, hmacSha256, persistCb, &p4ctr, &m) ==
               SHOWDUINO_DIR_OK && m.op == SHOWDUINO_DIR_OP_REQ && p4ctr.v == m.counter,
           "D1b P4 accepts the REQ and persists the counter first");
    ShowduinoClearTxn t{};
    expect(showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 9, 1000, testRand),
           "D1c P4 opens a transaction with a fresh nonce");
    char notice[64];
    showduino_dir_format_txn_notice(9, t.nonceHex, notice, sizeof(notice));
    expect(showduino_dir_parse_txn_notice(notice, &dir.txnEpisode, dir.txnHex),
           "D1d Director parses the TXN notice");
    expect(showduino_dir_sign_cfm(&dir, hmacSha256, persistCb, &dirctr, cfm, sizeof(cfm)),
           "D1e Director signs a CFM for that transaction");
    expect(showduino_dir_accept(&rec, cfm, 9, showduino_clear_txn_open_nonce(&t), hmacSha256,
                                persistCb, &p4ctr, &m) == SHOWDUINO_DIR_OK &&
           m.op == SHOWDUINO_DIR_OP_CFM, "D1f P4 accepts the CFM");
    expect(!showduino_dir_sign_cfm(&dir, hmacSha256, persistCb, &dirctr, cfm, sizeof(cfm)),
           "D1g Director will not sign a second CFM for the same transaction");
  }

  /* ===== D2: forgery, tampering, wrong key, wrong identity ===== */
  {
    ShowduinoDirKeyRec r = makeRec("DIR-01", 1);
    Store c;
    uint8_t wrong[32]; keyFill(wrong, 2);
    char line[128];
    showduino_dir_format(SHOWDUINO_DIR_OP_REQ, "DIR-01", 9, SHOWDUINO_DIR_TXN_ZERO, 100, wrong, 32,
                         hmacSha256, line, sizeof(line));
    expect(showduino_dir_accept(&r, line, 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_TAG && c.v == 0 && r.lastCounter == 0,
           "D2a forged REQ (wrong key) rejected; nothing persisted");
    showduino_dir_format(SHOWDUINO_DIR_OP_REQ, "DIR-01", 9, SHOWDUINO_DIR_TXN_ZERO, 100, r.key, 32,
                         hmacSha256, line, sizeof(line));
    std::string t = line;
    t[t.size() - 1] = t.back() == '0' ? '1' : '0';
    expect(showduino_dir_accept(&r, t.c_str(), 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_TAG, "D2b last-byte tag tamper rejected");
    std::string e = line;
    e.replace(e.find(":00000009:"), 10, ":0000000a:");
    expect(showduino_dir_accept(&r, e.c_str(), 10, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_TAG, "D2c valid tag with altered episode rejected");
    std::string k = line;
    k.replace(k.find(":00000064:"), 10, ":00000065:");
    expect(showduino_dir_accept(&r, k.c_str(), 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_TAG, "D2d valid tag with altered counter rejected");
    std::string o = line;
    o.replace(5, 3, "CFM");
    expect(showduino_dir_accept(&r, o.c_str(), 9, SHOWDUINO_DIR_TXN_ZERO, hmacSha256, persistCb, &c,
                                nullptr) != SHOWDUINO_DIR_OK, "D2e REQ re-labelled as CFM rejected");
    showduino_dir_format(SHOWDUINO_DIR_OP_REQ, "DIR-02", 9, SHOWDUINO_DIR_TXN_ZERO, 100, r.key, 32,
                         hmacSha256, line, sizeof(line));
    expect(showduino_dir_accept(&r, line, 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_IDENTITY, "D2f message from another Director ID rejected");
    ShowduinoDirKeyRec none{};
    expect(showduino_dir_accept(&none, line, 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_NO_KEY, "D2g no enrolled Director: nothing accepted");
    /* Domain separation: a station-format tag under the same key does not verify. */
    char canon[128];
    showduino_auth_canon_cmd(canon, sizeof(canon), "DIR-01", 100, "GCL", 9);
    uint8_t tag[16];
    showduino_auth_tag(hmacSha256, r.key, 32, canon, tag);
    char tagHex[33];
    showduino_auth_hex_encode(tag, 16, tagHex);
    std::string mixed = std::string("DCLR:REQ:DIR-01:00000009:0000000000000000:00000064:") + tagHex;
    expect(showduino_dir_accept(&r, mixed.c_str(), 9, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_TAG, "D2h station CMD authority cannot be reused as Director authority");
  }

  /* ===== D3: replay ===== */
  {
    ShowduinoDirKeyRec r = makeRec("DIR-01", 3);
    ShowduinoDirSigner d = makeSigner("DIR-01", 3);
    Store c, dc;
    d.episode = 4;
    char req[128];
    showduino_dir_sign_req(&d, hmacSha256, persistCb, &dc, req, sizeof(req));
    expect(showduino_dir_accept(&r, req, 4, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_OK, "D3a first delivery accepted");
    expect(showduino_dir_accept(&r, req, 4, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_REPLAY, "D3b exact replay rejected by the counter");
    /* P4 restart: the record (with lastCounter) is reloaded from storage. */
    ShowduinoDirKeyRec after = r;
    after.lastCounter = c.v;
    expect(showduino_dir_accept(&after, req, 4, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_REPLAY, "D3c replay after P4 restart rejected");
    /* Director restart: its counter resumes above the persisted block limit. */
    ShowduinoDirSigner d2 = makeSigner("DIR-01", 3, dc.v);
    d2.episode = 4;
    char req2[128];
    ShowduinoDirMsg m;
    showduino_dir_sign_req(&d2, hmacSha256, persistCb, &dc, req2, sizeof(req2));
    expect(showduino_dir_accept(&after, req2, 4, nullptr, hmacSha256, persistCb, &c, &m) ==
               SHOWDUINO_DIR_OK, "D3d after Director restart new messages are fresh");
    expect(showduino_dir_accept(&after, req, 4, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_REPLAY, "D3e and pre-restart traffic stays rejected");
    ShowduinoDirSigner d3 = makeSigner("DIR-01", 3, 1); /* Director lost its counter store */
    d3.episode = 4;
    showduino_dir_sign_req(&d3, hmacSha256, persistCb, &dc, req2, sizeof(req2));
    expect(showduino_dir_accept(&after, req2, 4, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_REPLAY,
           "D3f a Director that lost its counter cannot authorise until re-enrolled (fail closed)");
  }

  /* ===== D4: episode and transaction binding ===== */
  {
    ShowduinoDirKeyRec r = makeRec("DIR-01", 4);
    ShowduinoDirSigner d = makeSigner("DIR-01", 4);
    Store c, dc;
    d.episode = 20;
    char req[128], cfm[128];
    showduino_dir_sign_req(&d, hmacSha256, persistCb, &dc, req, sizeof(req));
    expect(showduino_dir_accept(&r, req, 21, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_EPISODE, "D4a REQ for another episode rejected");
    expect(showduino_dir_accept(&r, req, 0, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_EPISODE, "D4b nothing accepted while no episode is durable");
    ShowduinoClearTxn t{};
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 20, 0, testRand);
    d.txnEpisode = 20;
    std::snprintf(d.txnHex, sizeof(d.txnHex), "%s", t.nonceHex);
    showduino_dir_sign_cfm(&d, hmacSha256, persistCb, &dc, cfm, sizeof(cfm));
    ShowduinoClearTxn t2 = t;
    showduino_clear_txn_open(&t2, SHOWDUINO_CLEAR_BY_DIRECTOR, 20, 0, testRand); /* newer request */
    expect(showduino_dir_accept(&r, cfm, 20, showduino_clear_txn_open_nonce(&t2), hmacSha256,
                                persistCb, &c, nullptr) == SHOWDUINO_DIR_REJECT_TXN,
           "D4c CFM for a superseded transaction rejected");
    ShowduinoClearTxn idle{};
    expect(showduino_dir_accept(&r, cfm, 20, showduino_clear_txn_open_nonce(&idle), hmacSha256,
                                persistCb, &c, nullptr) == SHOWDUINO_DIR_REJECT_TXN,
           "D4d CFM with no open transaction rejected");
    expect(showduino_dir_accept(&r, cfm, 20, showduino_clear_txn_open_nonce(&t), hmacSha256,
                                persistCb, &c, nullptr) == SHOWDUINO_DIR_OK,
           "D4e CFM for the open transaction accepted");
    showduino_clear_txn_begin_verify(&t, 20, 10, 1);
    expect(showduino_clear_txn_open_nonce(&t) == nullptr,
           "D4f once verifying, the transaction nonce can no longer be confirmed (single use)");
  }

  /* ===== D5: restricted (enrolled during an active emergency) and persistence ===== */
  {
    ShowduinoDirKeyRec r = makeRec("DIR-01", 5);
    r.restricted = 1;
    ShowduinoDirSigner d = makeSigner("DIR-01", 5);
    Store c, dc;
    d.episode = 3;
    char req[128];
    showduino_dir_sign_req(&d, hmacSha256, persistCb, &dc, req, sizeof(req));
    expect(showduino_dir_accept(&r, req, 3, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_RESTRICTED && c.v == 0,
           "D5a a Director enrolled during the emergency cannot clear it");
    r.restricted = 0;
    c.fail = 1;
    expect(showduino_dir_accept(&r, req, 3, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_REJECT_PERSIST && r.lastCounter == 0,
           "D5b counter not durable: REQ refused, nothing recorded");
    expect(showduino_dir_accept(&r, req, 3, nullptr, hmacSha256, persistCb, &c, nullptr) ==
               SHOWDUINO_DIR_OK, "D5c accepted once storage works");
    ShowduinoDirSigner e = makeSigner("DIR-01", 5, 1);
    e.episode = 3;
    Store bad; bad.fail = 1;
    expect(!showduino_dir_sign_req(&e, hmacSha256, persistCb, &bad, req, sizeof(req)),
           "D5d Director signs nothing when its counter block cannot be stored");
    ShowduinoDirSigner unprov{};
    unprov.episode = 3;
    expect(!showduino_dir_sign_req(&unprov, hmacSha256, persistCb, &dc, req, sizeof(req)),
           "D5e an unprovisioned Director signs nothing");
  }

  /* ===== T: transaction state machine ===== */
  {
    ShowduinoClearTxn t{};
    expect(showduino_clear_txn_begin_verify(&t, 5, 0, 1) == SHOWDUINO_TXN_REJECT_NOT_OPEN,
           "T0 confirm without a request rejected");
    expect(!showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 0, 0, testRand),
           "T1 no transaction without a durable episode");
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 5, 100, testRand);
    expect(showduino_clear_txn_begin_verify(&t, 5, 100 + SHOWDUINO_CLEAR_TXN_OPEN_MS + 1, 1) ==
               SHOWDUINO_TXN_REJECT_EXPIRED && t.state == SHOWDUINO_TXN_IDLE,
           "T2 confirm after the request window rejected");
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 5, 100, testRand);
    expect(showduino_clear_txn_begin_verify(&t, 6, 200, 1) == SHOWDUINO_TXN_REJECT_EPISODE,
           "T3 confirm in a new episode rejected");
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 5, 100, testRand);
    expect(showduino_clear_txn_on_assert(&t) == SHOWDUINO_TXN_REJECT_SUPERSEDED &&
           t.state == SHOWDUINO_TXN_IDLE, "T4 any ASSERT cancels an open transaction");
    char first[17];
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 5, 100, testRand);
    std::memcpy(first, t.nonceHex, 17);
    showduino_clear_txn_open(&t, SHOWDUINO_CLEAR_BY_DIRECTOR, 5, 101, testRand);
    expect(std::strcmp(first, t.nonceHex) != 0, "T5 every request gets a new nonce");
    showduino_clear_txn_begin_verify(&t, 5, 200, 10);
    expect(!showduino_clear_txn_want_rechallenge(&t, 300) &&
           showduino_clear_txn_want_rechallenge(&t, 200 + SHOWDUINO_CLEAR_VERIFY_RECHAL_MS),
           "T6 re-challenge cadence while verifying");
  }

  /* ===== P: entry-point policy ===== */
  {
    expect(showduino_clear_entry(0, "EMERGENCY:CLEAR") == SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH &&
           showduino_clear_entry(0, "EMERGENCY:CLEAR_CONFIRM") == SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH &&
           showduino_clear_entry(0, "EMERGENCY:CLEAR_REQUEST") == SHOWDUINO_CLEAR_ENTRY_REFUSE_AUTH,
           "P0 plain clear text from the UART (Director, S3, web, show cue) is refused");
    expect(showduino_clear_entry(0, "DCLR:REQ:x") == SHOWDUINO_CLEAR_ENTRY_DIRECTOR,
           "P1 DCLR from the UART is authenticated as Director");
    expect(showduino_clear_entry(1, "EMERGENCY:CLEAR") == SHOWDUINO_CLEAR_ENTRY_LOCAL,
           "P2 USB console clear is the local maintenance authority");
    expect(showduino_clear_entry(1, "DCLR:CFM:x") == SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE &&
           showduino_clear_entry(1, "EMERGENCY:CLEAR_CONFIRM") == SHOWDUINO_CLEAR_ENTRY_REFUSE_SOURCE,
           "P3 Director messages or Director confirms are refused on USB");
    expect(showduino_clear_entry(0, "EMERGENCY:CLEAR_CANCEL") == SHOWDUINO_CLEAR_ENTRY_CANCEL &&
           showduino_clear_entry(1, "EMERGENCY:CLEAR_CANCEL") == SHOWDUINO_CLEAR_ENTRY_CANCEL,
           "P4 cancel is allowed from anywhere (it can only cancel)");
    expect(showduino_clear_entry(0, "EMERGENCY:STOP") == SHOWDUINO_CLEAR_ENTRY_NONE,
           "P5 activation is not touched by the clear policy");
  }

  std::printf("\n%d passed, %d failed\n", gPass, gFails);
  std::printf(gFails ? "FAILURES\n" : "ALL PASS\n");
  return gFails ? 1 : 0;
}
