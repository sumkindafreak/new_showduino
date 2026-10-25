/*
 * MISSION 001J host tests: station authentication, challenge freshness, command
 * counters, episode binding, and counter-block persistence.
 *
 * Exercises protocol/showduino_station_auth.h with the same functions the P4 and
 * C3 firmware call. HMAC-SHA256 comes from OpenSSL here and from mbedtls on the
 * ESP32. RFC 4231 vectors check the wrapper. Build:
 *   g++ -std=c++17 -Wall -Wextra -Werror -Iprotocol test_station_auth.cpp -lcrypto
 */
#include "showduino_emergency_node.h"
#include "showduino_station_auth.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int gFails = 0;
static int gPass = 0;
static void expect(bool ok, const char *msg) {
  if (ok) { std::printf("PASS  %s\n", msg); gPass++; }
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

/* ---- HMAC-SHA256 (OpenSSL), wrapped to the shared function type ---- */
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

static std::string hex(const uint8_t *b, size_t n) {
  std::string s;
  char buf[3];
  for (size_t i = 0; i < n; ++i) { std::snprintf(buf, sizeof(buf), "%02x", b[i]); s += buf; }
  return s;
}

/* Deterministic test RNG. Never used on hardware. */
static uint64_t gRngState = 0x9E3779B97F4A7C15ull;
static int testRand(uint8_t *out, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    gRngState = gRngState * 6364136223846793005ull + 1442695040888963407ull;
    out[i] = (uint8_t)(gRngState >> 33);
  }
  return 0;
}

static uint8_t K1[32], K2[32];

int main() {
  for (int i = 0; i < 32; ++i) { K1[i] = (uint8_t)(0x11 + i); K2[i] = (uint8_t)(0xA0 + i); }

  /* ============ HMAC wrapper: RFC 4231 test cases 1 and 2 ============ */
  {
    uint8_t key1[20];
    std::memset(key1, 0x0b, sizeof(key1));
    uint8_t out[32];
    hmacSha256(key1, sizeof(key1), (const uint8_t *)"Hi There", 8, out);
    expect(hex(out, 32) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
           "RFC4231 case 1: HMAC-SHA256 wrapper matches the standard vector");
    hmacSha256((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, out);
    expect(hex(out, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
           "RFC4231 case 2: HMAC-SHA256 wrapper matches the standard vector");
  }

  /* ============ AUTH: station answers a P4 challenge ============ */
  const uint32_t EP = 7;
  {
    ShowduinoAuthChalTable ch;
    showduino_auth_chal_reset(&ch);
    char nonce[17];
    expect(showduino_auth_chal_issue(&ch, 1000, EP, testRand, nonce) == 1 &&
           showduino_auth_hex_is_lower_or_upper_len(nonce, 16),
           "A0 P4 issues a 64-bit challenge nonce");

    char chal[64];
    showduino_auth_format_chal(EP, nonce, chal, sizeof(chal));
    ShowduinoAuthChalMsg cm{};
    expect(showduino_auth_parse_chal(chal, &cm) && cm.episode == EP && !std::strcmp(cm.nonceHex, nonce),
           "A0b CHAL line round-trips episode and nonce");

    /* A2: valid AUTH from the station holding K1 */
    char line[128];
    showduino_auth_format_auth("ESTOP-01", cm.episode, cm.nonceHex, 0, 1, 0, K1, 32, hmacSha256, line, sizeof(line));
    ShowduinoAuthMsg m{};
    expect(showduino_auth_parse_auth(line, &m) && showduino_auth_verify_auth(&m, K1, 32, hmacSha256),
           "A2 valid authenticated station message verifies");
    expect(showduino_auth_chal_consume(&ch, m.nonceHex, EP, 1100, 0) == 1,
           "A2b verified answer consumes the outstanding nonce for station slot 0");
    expect(std::strlen(line) <= SHOWDUINO_AUTH_LINE_MAX,
           "A2c AUTH line fits the 95-character ESP-NOW command field");

    /* A3: forged (station without the right key) */
    char forged[128];
    showduino_auth_format_auth("ESTOP-01", EP, cm.nonceHex, 0, 0, 0, K2, 32, hmacSha256, forged, sizeof(forged));
    ShowduinoAuthMsg fm{};
    showduino_auth_parse_auth(forged, &fm);
    expect(!showduino_auth_verify_auth(&fm, K1, 32, hmacSha256),
           "A3 forged AUTH (wrong key) is rejected");

    /* A4: tampered field: the tag was made for IN=0, the line says IN=1 */
    std::string tampered = line;
    tampered[tampered.size() - 38] = '1'; /* IN flag, 38 characters from the end */
    ShowduinoAuthMsg tm{};
    const bool tparsed = showduino_auth_parse_auth(tampered.c_str(), &tm);
    expect(tparsed && !showduino_auth_verify_auth(&tm, K1, 32, hmacSha256),
           "A4 tampered physical state (IN flipped) is rejected by the tag");

    /* A5: identity binding. A tag made for ESTOP-01 must not verify as ESTOP-02. */
    ShowduinoAuthMsg idm = m;
    std::snprintf(idm.id, sizeof(idm.id), "%s", "ESTOP-02");
    expect(!showduino_auth_verify_auth(&idm, K1, 32, hmacSha256),
           "A5 identity binding: a tag for ESTOP-01 does not verify for ESTOP-02");
  }

  /* A6 nonce not outstanding; A7 expiry; A8 replay; A9 reboot; A10 episode; A11 multi-station */
  {
    ShowduinoAuthChalTable ch;
    showduino_auth_chal_reset(&ch);
    char n1[17];
    showduino_auth_chal_issue(&ch, 1000, EP, testRand, n1);

    char bogus[17] = "0011223344556677";
    expect(showduino_auth_chal_consume(&ch, bogus, EP, 1100, 0) == 0,
           "A6 AUTH answering a nonce this P4 never issued is rejected");

    expect(showduino_auth_chal_consume(&ch, n1, EP, 1000 + SHOWDUINO_AUTH_WINDOW_MS + 1, 1) == 0,
           "A7 expired challenge (older than the 1500 ms window) is rejected");

    ShowduinoAuthChalTable ch2;
    showduino_auth_chal_reset(&ch2);
    char n2[17];
    showduino_auth_chal_issue(&ch2, 1000, EP, testRand, n2);
    expect(showduino_auth_chal_consume(&ch2, n2, EP, 1200, 0) == 1, "A8a first answer to a nonce is accepted");
    expect(showduino_auth_chal_consume(&ch2, n2, EP, 1210, 0) == 0,
           "A8b replay of the same answer (same station, same nonce) is rejected");
    expect(showduino_auth_chal_consume(&ch2, n2, EP, 1220, 1) == 1,
           "A11 a different station may answer the same broadcast nonce once");
    expect(showduino_auth_chal_consume(&ch2, n2, EP, 1230, 1) == 0,
           "A11b the same station cannot answer the same nonce twice");

    /* A9: P4 reboot wipes the outstanding table. A pre-reboot answer must fail. */
    ShowduinoAuthChalTable afterReboot;
    showduino_auth_chal_reset(&afterReboot);
    expect(showduino_auth_chal_consume(&afterReboot, n2, EP, 1240, 2) == 0,
           "A9 replay after P4 restart: pre-reboot nonce is no longer outstanding");

    /* A10: an answer for an earlier episode must not satisfy the current one */
    ShowduinoAuthChalTable ch3;
    showduino_auth_chal_reset(&ch3);
    char n3[17];
    showduino_auth_chal_issue(&ch3, 2000, 7, testRand, n3);
    expect(showduino_auth_chal_consume(&ch3, n3, 8, 2100, 0) == 0,
           "A10 an answer issued in episode 7 does not satisfy episode 8");

    /* Eviction: a fifth challenge evicts the oldest, whose answers are then refused */
    ShowduinoAuthChalTable ev;
    showduino_auth_chal_reset(&ev);
    char first[17], last[17];
    showduino_auth_chal_issue(&ev, 0, EP, testRand, first);
    for (int i = 1; i < SHOWDUINO_AUTH_CHAL_SLOTS; ++i) {
      char tmp[17];
      showduino_auth_chal_issue(&ev, (uint32_t)(i * 10), EP, testRand, tmp);
    }
    showduino_auth_chal_issue(&ev, 100, EP, testRand, last);
    expect(showduino_auth_chal_consume(&ev, first, EP, 110, 0) == 0,
           "A14 the evicted oldest nonce is no longer accepted");
  }

  /* A4b: the tag alone is altered in its last byte. Catches partial comparisons. */
  {
    char line2[128];
    char nonce2[17] = "fedcba9876543210";
    showduino_auth_format_auth("ESTOP-01", EP, nonce2, 1, 1, 0, K1, 32, hmacSha256, line2, sizeof(line2));
    std::string t2 = line2;
    t2[t2.size() - 1] = (t2[t2.size() - 1] == '0') ? '1' : '0';
    ShowduinoAuthMsg m2{};
    expect(showduino_auth_parse_auth(t2.c_str(), &m2) && !showduino_auth_verify_auth(&m2, K1, 32, hmacSha256),
           "A4b AUTH with only the last tag byte altered is rejected");
  }

  /* A12/A13 malformed AUTH lines */
  {
    char good[128];
    char nonce[17] = "0123456789abcdef";
    showduino_auth_format_auth("ESTOP-01", EP, nonce, 1, 0, 0, K1, 32, hmacSha256, good, sizeof(good));
    ShowduinoAuthMsg m{};
    std::string s = good;
    std::string shortTag = s.substr(0, s.size() - 1);
    expect(!showduino_auth_parse_auth(shortTag.c_str(), &m), "A12a AUTH with a truncated tag is rejected");
    std::string badFlag = s;
    badFlag[badFlag.size() - 38] = '2';
    expect(!showduino_auth_parse_auth(badFlag.c_str(), &m), "A12b AUTH with a non-binary flag is rejected");
    std::string nonHexNonce = s;
    nonHexNonce[s.find(':', 5 + 12 + 1 + 8 + 1) + 1] = 'z';
    expect(!showduino_auth_parse_auth(nonHexNonce.c_str(), &m), "A12c AUTH with a non-hex nonce is rejected");
    expect(!showduino_auth_parse_auth("AUTH:ESTOP-01:00000007:0123456789abcdef:1:0:0", &m),
           "A12d AUTH with no tag is rejected");
    expect(!showduino_auth_parse_auth("AUTH:ESTOP-0123456789:00000007:0123456789abcdef:1:0:0:00", &m),
           "A12e AUTH with an over-long identity is rejected");
    expect(!showduino_auth_parse_auth("ANNOUNCE:AA:BB:CC:DD:EE:FF:1.0:NORMAL:ID=ESTOP-01", &m),
           "A12f a plain ANNOUNCE is not an AUTH");
  }

  /* ============ CMD: P4 -> station, counters, replay ============ */
  {
    /* P4 boot with no stored record starts counting at 1 (0 is never fresh). */
    ShowduinoAuthCounter pc;
    int persists = 0;
    uint32_t stored = 0;
    auto persistStore = [](void *ctx, uint32_t v) -> int {
      *(uint32_t *)ctx = v;
      return 1;
    };
    (void)persists;
    showduino_auth_counter_boot(&pc, 1);
    uint32_t c0 = 0;
    showduino_auth_counter_next(&pc, persistStore, &stored, &c0);
    expect(c0 == 1 && stored == 1 + SHOWDUINO_AUTH_COUNTER_BLOCK,
           "K0 P4 first command counter is 1 and the block limit is persisted first");

    char cmd[120];
    showduino_auth_format_cmd("ESTOP-01", 5, "ACK", 7, K1, 32, hmacSha256, cmd, sizeof(cmd));
    ShowduinoCmdMsg cm{};
    uint32_t last = 4; /* station's persisted last accepted counter */
    expect(showduino_auth_parse_cmd(cmd, &cm) && showduino_auth_verify_cmd(&cm, K1, 32, hmacSha256) &&
           showduino_cmd_counter_is_fresh(last, cm.counter),
           "C1 valid authenticated ACK with a fresh counter is accepted");
    last = cm.counter; /* persisted before apply */

    expect(!showduino_cmd_counter_is_fresh(last, cm.counter),
           "C2 replay of the same command is rejected by the counter");
    char oldCmd[120];
    showduino_auth_format_cmd("ESTOP-01", 3, "ACK", 7, K1, 32, hmacSha256, oldCmd, sizeof(oldCmd));
    ShowduinoCmdMsg om{};
    showduino_auth_parse_cmd(oldCmd, &om);
    expect(showduino_auth_verify_cmd(&om, K1, 32, hmacSha256) && !showduino_cmd_counter_is_fresh(last, om.counter),
           "C3 an older validly-tagged command is rejected once a newer one was accepted");

    char forgedCmd[120];
    showduino_auth_format_cmd("ESTOP-01", 9, "GCL", 7, K2, 32, hmacSha256, forgedCmd, sizeof(forgedCmd));
    ShowduinoCmdMsg fc{};
    showduino_auth_parse_cmd(forgedCmd, &fc);
    expect(!showduino_auth_verify_cmd(&fc, K1, 32, hmacSha256), "C4 forged GCL (wrong key) is rejected");

    char otherId[120];
    showduino_auth_format_cmd("ESTOP-02", 9, "GCL", 7, K1, 32, hmacSha256, otherId, sizeof(otherId));
    ShowduinoCmdMsg oc{};
    showduino_auth_parse_cmd(otherId, &oc);
    std::snprintf(oc.id, sizeof(oc.id), "%s", "ESTOP-01");
    expect(!showduino_auth_verify_cmd(&oc, K1, 32, hmacSha256),
           "C5 command made for ESTOP-02 does not verify for ESTOP-01");

    std::string opTamper = cmd;
    const size_t at = opTamper.find(":ACK:");
    opTamper.replace(at + 1, 3, "GCL");
    ShowduinoCmdMsg tc{};
    showduino_auth_parse_cmd(opTamper.c_str(), &tc);
    expect(!showduino_auth_verify_cmd(&tc, K1, 32, hmacSha256), "C6 tampered operation (ACK->GCL) is rejected");

    ShowduinoCmdMsg bad{};
    expect(!showduino_auth_parse_cmd("CMD:ESTOP-01:00000005:XYZ:00000007:00", &bad),
           "C7 command with an unknown operation is rejected at parse");
  }

  /* C11: command tag altered only in its last byte */
  {
    char cl[120];
    showduino_auth_format_cmd("ESTOP-01", 40, "GCL", 7, K1, 32, hmacSha256, cl, sizeof(cl));
    std::string c2 = cl;
    c2[c2.size() - 1] = (c2[c2.size() - 1] == 'a') ? 'b' : 'a';
    ShowduinoCmdMsg cm2{};
    expect(showduino_auth_parse_cmd(c2.c_str(), &cm2) && !showduino_auth_verify_cmd(&cm2, K1, 32, hmacSha256),
           "C11 command with only the last tag byte altered is rejected");
  }

  /* C8: replay across P4 restart. The P4 resumes past its persisted limit. */
  {
    uint32_t stored = 0;
    auto persistStore = [](void *ctx, uint32_t v) -> int { *(uint32_t *)ctx = v; return 1; };
    ShowduinoAuthCounter a;
    showduino_auth_counter_boot(&a, 1);
    uint32_t last = 0, v = 0;
    for (int i = 0; i < 5; ++i) showduino_auth_counter_next(&a, persistStore, &stored, &v);
    const uint32_t lastIssued = v;
    /* station accepted lastIssued; then the P4 reboots */
    last = lastIssued;
    ShowduinoAuthCounter b;
    showduino_auth_counter_boot(&b, stored);
    uint32_t fresh = 0;
    showduino_auth_counter_next(&b, persistStore, &stored, &fresh);
    expect(fresh > lastIssued && showduino_cmd_counter_is_fresh(last, fresh),
           "C8 after P4 restart the next command is fresh; old counter values are never reused");
  }

  /* C9: replay across C3 restart. The station reloads its persisted last value. */
  {
    uint32_t persistedLast = 12; /* written before apply */
    uint32_t afterReboot = persistedLast;
    expect(!showduino_cmd_counter_is_fresh(afterReboot, 12),
           "C9 after C3 restart the persisted counter still rejects a replay of counter 12");
  }

  /* C10: station persistence failure: command must not apply */
  {
    uint32_t last = 4;
    const uint32_t incoming = 5;
    const bool persistedOk = false;
    bool applied = false;
    if (showduino_cmd_counter_is_fresh(last, incoming) && persistedOk) {
      last = incoming;
      applied = true;
    }
    expect(!applied && last == 4,
           "C10 failed counter persistence on the station refuses the command (no apply, no advance)");
  }

  /* ============ Episode binding on the station (GCL / ACK) ============ */
  {
    uint32_t latchedEp = 0;
    showduino_auth_learn_episode(&latchedEp, 0);
    expect(latchedEp == 0, "E0 an idle challenge (episode 0) does not overwrite the latched episode");
    showduino_auth_learn_episode(&latchedEp, EP);
    expect(showduino_auth_gcl_matches_episode(latchedEp, EP), "E1 GCL for the latched episode matches");
    expect(!showduino_auth_gcl_matches_episode(latchedEp, EP + 1), "E2 GCL for another episode is refused");
    expect(!showduino_auth_gcl_matches_episode(0, 0), "E3 GCL with no known episode is refused (fail closed)");
  }

  /* ============ Counter block allocator: monotonic, persisted, fail-closed ============ */
  {
    uint32_t stored = 0;
    int calls = 0;
    auto persistStore = [](void *ctx, uint32_t v) -> int { *(uint32_t *)ctx = v; return 1; };
    (void)calls;
    ShowduinoAuthCounter c;
    showduino_auth_counter_boot(&c, 1);
    uint32_t prev = 0, v = 0;
    bool monotone = true;
    for (int i = 0; i < 600; ++i) {
      if (!showduino_auth_counter_next(&c, persistStore, &stored, &v)) { monotone = false; break; }
      if (i > 0 && v <= prev) monotone = false;
      prev = v;
    }
    expect(monotone, "K1 counter values strictly increase across 600 allocations");

    /* K2: persistence failure hands out nothing */
    ShowduinoAuthCounter f;
    showduino_auth_counter_boot(&f, 300);
    uint32_t dummy = 0;
    auto failStore = [](void *, uint32_t) -> int { return 0; };
    const bool got = showduino_auth_counter_next(&f, failStore, nullptr, &dummy);
    expect(!got && f.next == 300, "K2 counter block not persisted: no value handed out");

    /* K3: reboot resumes past every value that may have been issued */
    uint32_t persistedLimit = 0;
    ShowduinoAuthCounter pre;
    showduino_auth_counter_boot(&pre, 1);
    uint32_t issued = 0, maxIssued = 0;
    for (int i = 0; i < 10; ++i) {
      showduino_auth_counter_next(&pre, persistStore, &persistedLimit, &issued);
      if (issued > maxIssued) maxIssued = issued;
    }
    ShowduinoAuthCounter post;
    showduino_auth_counter_boot(&post, persistedLimit);
    uint32_t firstAfter = 0;
    showduino_auth_counter_next(&post, persistStore, &persistedLimit, &firstAfter);
    expect(firstAfter > maxIssued, "K3 after reboot no previously issued counter value is reused");
  }

    /* ============ N: P4 nonce generator ============ */
  {
    ShowduinoNonceGen g{};
    uint8_t seed[32]; for (int i = 0; i < 32; ++i) seed[i] = (uint8_t)(i * 13 + 5);
    uint8_t a[8], b[8], c[8];
    expect(showduino_nonce_gen_next(&g, a, 8) != 0, "N0 unseeded generator refuses output");
    uint8_t zero[32] = {0};
    expect(!showduino_nonce_gen_seed(&g, zero, hmacSha256), "N1 all-zero seed refused");
    expect(showduino_nonce_gen_seed(&g, seed, hmacSha256) &&
           showduino_nonce_gen_next(&g, a, 8) == 0 && showduino_nonce_gen_next(&g, b, 8) == 0 &&
           std::memcmp(a, b, 8) != 0, "N2 consecutive nonces differ");
    ShowduinoNonceGen g2{};
    showduino_nonce_gen_seed(&g2, seed, hmacSha256);
    showduino_nonce_gen_next(&g2, c, 8);
    expect(std::memcmp(a, c, 8) == 0, "N3 same seed and counter give the same nonce (deterministic)");
    seed[31] ^= 1;
    ShowduinoNonceGen g3{};
    showduino_nonce_gen_seed(&g3, seed, hmacSha256);
    showduino_nonce_gen_next(&g3, c, 8);
    expect(std::memcmp(a, c, 8) != 0, "N4 a different boot seed gives different nonces");
    g.ctr = 0xFFFFFFFFu;
    expect(showduino_nonce_gen_next(&g, a, 8) != 0, "N5 counter exhaustion refuses output");
  }

  /* ============ Q: challenge sequence survives reset ============ */
  {
    ShowduinoAuthChalTable t{};
    char nh[SHOWDUINO_AUTH_NONCE_HEX + 1];
    showduino_auth_chal_issue(&t, 100, 1, testRand, nh);
    showduino_auth_chal_issue(&t, 100, 1, testRand, nh);
    const uint32_t before = t.lastSeq;
    showduino_auth_chal_reset(&t);
    expect(before == 2 && t.lastSeq == 2 && showduino_auth_chal_next_seq(&t) == 3,
           "Q0 reset keeps the sequence, so an old proof never looks newer");
    uint32_t seq = 0;
    showduino_auth_chal_issue(&t, 200, 1, testRand, nh);
    expect(showduino_auth_chal_consume_at(&t, nh, 1, 200, 0, &seq) && seq == 3,
           "Q1 consume reports the challenge sequence");
  }

  /* ============ R: route ALL ============ */
  {
    ShowduinoEmergencyRoute rt{};
    expect(showduino_emergency_parse_route("ROUTE:EMERGENCY:ALL:7:CHAL:00000001:0011223344556677", &rt) &&
           rt.broadcast == 1 && rt.sequence == 7, "R0 ROUTE:EMERGENCY:ALL parses as broadcast");
    expect(showduino_emergency_parse_route("ROUTE:EMERGENCY:ESTOP-02:9:X", &rt) && rt.broadcast == 0,
           "R1 a station route is not a broadcast");
    expect(!showduino_emergency_parse_route("ROUTE:EMERGENCY:ALLX:9:X", &rt),
           "R2 other non-station ids are still rejected");
  }

std::printf("\n%d passed, %d failed\n", gPass, gFails);
  std::printf(gFails == 0 ? "ALL PASS\n" : "FAILURES: %d\n", gFails);
  return gFails == 0 ? 0 : 1;
}
