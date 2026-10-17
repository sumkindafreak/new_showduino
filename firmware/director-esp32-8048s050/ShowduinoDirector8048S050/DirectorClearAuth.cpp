#include "DirectorClearAuth.h"
#include <Preferences.h>
#include "../../../protocol/showduino_director_auth.h"
#include "../../../protocol/showduino_station_auth_esp32.h"

/* NVS namespace "dir_auth":
 *   kblob : KeyRec { magic, id, key[32], crc32 }
 *   ctr   : uint32 counter-block limit (persist-before-use, like the P4) */
static const char *kNs = "dir_auth";
static const uint32_t kMagic = 0x31594B52u; /* "RKY1" */

struct KeyRec {
  uint32_t magic;
  char id[SHOWDUINO_DIR_ID_MAX + 1];
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  uint32_t crc;
};

static ShowduinoDirSigner sSigner;
static bool sFault = false;

static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

static uint32_t recCrc(const KeyRec &r) {
  return crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(KeyRec, crc));
}

static int persistCtrCb(void *ctx, uint32_t value) {
  (void)ctx;
  Preferences p;
  if (!p.begin(kNs, false)) return 0;
  const size_t w = p.putUInt("ctr", value);
  const uint32_t back = p.getUInt("ctr", 0);
  p.end();
  return (w == sizeof(uint32_t) && back == value) ? 1 : 0;
}

void directorClearAuthBegin() {
  memset(&sSigner, 0, sizeof(sSigner));
  sFault = false;
  Preferences p;
  if (!p.begin(kNs, false)) {
    sFault = true;
    Serial.println("[DIR-AUTH] NVS unavailable — Director cannot authorise an emergency clear");
    return;
  }
  uint32_t limit = p.getUInt("ctr", 1);
  if (limit == 0) limit = 1;
  if (p.isKey("kblob")) {
    KeyRec r;
    const size_t n = p.getBytes("kblob", &r, sizeof(r));
    if (n == sizeof(r) && r.magic == kMagic && r.crc == recCrc(r) && showduino_dir_id_ok(r.id)) {
      sSigner.provisioned = 1;
      memcpy(sSigner.id, r.id, sizeof(sSigner.id));
      memcpy(sSigner.key, r.key, sizeof(sSigner.key));
    } else {
      sFault = true;
    }
    memset(&r, 0, sizeof(r));
  }
  p.end();
  showduino_auth_counter_boot(&sSigner.ctr, limit);
  if (sFault) {
    Serial.println("[DIR-AUTH] key record unreadable — enrol again (PROVISION DIRECTOR ...)");
  } else if (!sSigner.provisioned) {
    Serial.println("[DIR-AUTH] NOT ENROLLED — this Director cannot clear an emergency");
  } else {
    Serial.printf("[DIR-AUTH] enrolled as %s\n", sSigner.id);
  }
}

bool directorClearAuthProvisioned() { return sSigner.provisioned != 0 && !sFault; }

static void fingerprint(char out[9]) {
  out[0] = 0;
  if (!sSigner.provisioned) return;
  static const char kMsg[] = "SHOWDUINO-KEY-FP|v1";
  uint8_t mac[32];
  if (showduino_auth_hmac_sha256_mbedtls(sSigner.key, sizeof(sSigner.key), (const uint8_t *)kMsg,
                                         sizeof(kMsg) - 1, mac) == 0) {
    showduino_auth_hex_encode(mac, 4, out);
  }
  memset(mac, 0, sizeof(mac));
}

static bool install(const char *id, const uint8_t *key) {
  if (!showduino_dir_id_ok(id)) {
    Serial.println("[DIR-AUTH] REFUSED — ID must be DIR-<1..4 digits>");
    return false;
  }
  const bool sameKey = sSigner.provisioned &&
                       showduino_auth_ct_equal(sSigner.key, key, SHOWDUINO_AUTH_KEY_BYTES) &&
                       strcmp(sSigner.id, id) == 0;
  KeyRec r;
  memset(&r, 0, sizeof(r));
  r.magic = kMagic;
  strncpy(r.id, id, sizeof(r.id) - 1);
  memcpy(r.key, key, sizeof(r.key));
  r.crc = recCrc(r);
  Preferences p;
  bool ok = p.begin(kNs, false);
  if (ok) {
    const size_t w = p.putBytes("kblob", &r, sizeof(r));
    KeyRec back;
    const size_t n = p.getBytes("kblob", &back, sizeof(back));
    ok = w == sizeof(r) && n == sizeof(r) && memcmp(&back, &r, sizeof(r)) == 0;
    memset(&back, 0, sizeof(back));
    p.end();
  }
  memset(&r, 0, sizeof(r));
  if (!ok) {
    Serial.println("[DIR-AUTH] FAILED — key not stored (verify failed)");
    return false;
  }
  /* A different key starts a new message history on the P4 (its counter is reset by
   * EMERGENCY:DIRKEY:SET); the same key keeps the counter. */
  const uint32_t keep = sSigner.ctr.limit;
  memset(&sSigner, 0, sizeof(sSigner));
  sSigner.provisioned = 1;
  strncpy(sSigner.id, id, sizeof(sSigner.id) - 1);
  memcpy(sSigner.key, key, sizeof(sSigner.key));
  if (!sameKey && !persistCtrCb(nullptr, 1)) {
    Serial.println("[DIR-AUTH] FAILED — counter not stored");
    sSigner.provisioned = 0;
    return false;
  }
  showduino_auth_counter_boot(&sSigner.ctr, sameKey ? keep : 1);
  sFault = false;
  return true;
}

static void printP4Line(const uint8_t *key) {
  char hex[SHOWDUINO_AUTH_KEY_HEX + 1];
  showduino_auth_hex_encode(key, SHOWDUINO_AUTH_KEY_BYTES, hex);
  char fp[9];
  fingerprint(fp);
  /* USB only, once. Never to the UI log, SD or the Stage link. */
  Serial.printf("[DIR-AUTH] Director %s fp=%s\n", sSigner.id, fp);
  Serial.printf("[DIR-AUTH] On the P4 USB console enter:\nEMERGENCY:DIRKEY:SET:%s:%s\n", sSigner.id, hex);
  memset(hex, 0, sizeof(hex));
}

bool directorClearAuthHandleUsb(const String &line) {
  if (!line.startsWith("PROVISION")) return false;
  String a = line.substring(9);
  a.trim();
  if (!a.startsWith("DIRECTOR")) {
    Serial.println("[DIR-AUTH] usage: PROVISION DIRECTOR NEW|SET|STATUS|CLEAR");
    return true;
  }
  a = a.substring(8);
  a.trim();
  if (a == "STATUS") {
    char fp[9];
    fingerprint(fp);
    Serial.printf("[DIR-AUTH] id=%s enrolled=%s fp=%s fault=%u counterLimit=%lu episode=%08lx\n",
                  sSigner.provisioned ? sSigner.id : "-", sSigner.provisioned ? "yes" : "no",
                  fp[0] ? fp : "-", sFault ? 1U : 0U, (unsigned long)sSigner.ctr.limit,
                  (unsigned long)sSigner.episode);
    return true;
  }
  if (a == "CLEAR") {
    Preferences p;
    bool ok = p.begin(kNs, false);
    if (ok) {
      p.remove("kblob");
      ok = !p.isKey("kblob");
      p.end();
    }
    if (ok) {
      memset(sSigner.key, 0, sizeof(sSigner.key));
      sSigner.provisioned = 0;
    }
    Serial.printf("[DIR-AUTH] key %s — revoke it on the P4 too (EMERGENCY:DIRKEY:REVOKE)\n",
                  ok ? "CLEARED" : "CLEAR FAILED");
    return true;
  }
  if (a.startsWith("NEW ")) {
    String id = a.substring(4);
    id.trim();
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    showduino_auth_rand_esp32(key, sizeof(key)); /* true RNG while Wi-Fi is running */
    int nonzero = 0;
    for (size_t i = 0; i < sizeof(key); ++i) nonzero |= key[i];
    if (nonzero && install(id.c_str(), key)) printP4Line(key);
    memset(key, 0, sizeof(key));
    return true;
  }
  if (a.startsWith("SET ")) {
    String rest = a.substring(4);
    rest.trim();
    const int sp = rest.indexOf(' ');
    if (sp <= 0 || rest.length() - (unsigned)sp - 1 != SHOWDUINO_AUTH_KEY_HEX) {
      Serial.println("[DIR-AUTH] usage: PROVISION DIRECTOR SET DIR-nn <64 hex chars>");
      return true;
    }
    const String id = rest.substring(0, sp);
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    const bool ok = showduino_auth_hex_decode(rest.c_str() + sp + 1, sizeof(key), key) &&
                    install(id.c_str(), key);
    memset(key, 0, sizeof(key));
    char fp[9];
    fingerprint(fp);
    Serial.printf("[DIR-AUTH] SET %s %s fp=%s\n", id.c_str(), ok ? "OK" : "FAILED", ok ? fp : "-");
    return true;
  }
  Serial.println("[DIR-AUTH] PROVISION DIRECTOR NEW DIR-nn | SET DIR-nn <hex64> | STATUS | CLEAR");
  return true;
}

void directorClearAuthOnStageLine(const String &line) {
  uint32_t e = 0;
  if (showduino_dir_parse_episode(line.c_str(), &e)) {
    if (e != sSigner.episode) sSigner.txnHex[0] = '\0'; /* a new episode voids any transaction */
    sSigner.episode = e;
    return;
  }
  showduino_dir_parse_txn_notice(line.c_str(), &sSigner.txnEpisode, sSigner.txnHex);
}

bool directorClearAuthSignRequest(String &out, String &why) {
  if (!directorClearAuthProvisioned()) { why = "Director not enrolled - clear from the P4 USB console"; return false; }
  if (sSigner.episode == 0) { why = "No emergency episode reported by the Stage yet"; return false; }
  char buf[SHOWDUINO_DIR_LINE_MAX + 1];
  if (!showduino_dir_sign_req(&sSigner, showduino_auth_hmac_sha256_mbedtls, persistCtrCb, nullptr, buf,
                              sizeof(buf))) {
    why = "Director key storage failed - clear not sent";
    return false;
  }
  out = buf;
  return true;
}

bool directorClearAuthSignConfirm(String &out, String &why) {
  if (!directorClearAuthProvisioned()) { why = "Director not enrolled"; return false; }
  if (!sSigner.txnHex[0]) { why = "No clear request is open - press E-CLEAR again"; return false; }
  char buf[SHOWDUINO_DIR_LINE_MAX + 1];
  if (!showduino_dir_sign_cfm(&sSigner, showduino_auth_hmac_sha256_mbedtls, persistCtrCb, nullptr, buf,
                              sizeof(buf))) {
    why = "Director key storage failed - confirm not sent";
    return false;
  }
  out = buf;
  return true;
}
