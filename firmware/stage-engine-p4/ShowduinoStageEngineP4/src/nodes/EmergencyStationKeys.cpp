#include "EmergencyStationKeys.h"
#include <Preferences.h>
#include "../../../protocol/showduino_emergency_latch.h"
#include "../../../protocol/showduino_station_auth_esp32.h"

/* NVS layout (namespace SHOWDUINO_LATCH_NVS_NAMESPACE):
 *   estop_keys : KeyBlob (binary, CRC32 over everything before crc)
 *   estop_ctr  : uint32 counter-block limit */
static const char *kKeysNvs = "estop_keys";
static const char *kCtrNvs = "estop_ctr";
static const uint32_t kMagic = 0x31594B53u; /* "SKY1" */

struct KeyEntry {
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
};

struct KeyBlob {
  uint32_t magic;
  uint8_t count;
  uint8_t reserved[3];
  KeyEntry e[SHOWDUINO_STATION_KEYS_MAX];
  uint32_t crc;
};

static KeyBlob sBlob;
static bool sFault = false;
static bool directorKeyEquals(const uint8_t *key);
static void directorKeyLoad();

static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

static uint32_t blobCrc(const KeyBlob &b) {
  return crc32(reinterpret_cast<const uint8_t *>(&b), offsetof(KeyBlob, crc));
}

static bool blobValid(const KeyBlob &b) {
  if (b.magic != kMagic || b.count > SHOWDUINO_STATION_KEYS_MAX) return false;
  if (b.crc != blobCrc(b)) return false;
  for (uint8_t i = 0; i < b.count; ++i) {
    if (!showduino_emergency_id_ok(b.e[i].id)) return false;
  }
  return true;
}

static void blobInit(KeyBlob &b) {
  memset(&b, 0, sizeof(b));
  b.magic = kMagic;
  b.crc = blobCrc(b);
}

/* Writes and reads back. Returns true only on a byte-exact match. */
static bool blobSave(const KeyBlob &b) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const size_t w = p.putBytes(kKeysNvs, &b, sizeof(b));
  static KeyBlob back;
  const size_t r = p.getBytes(kKeysNvs, &back, sizeof(back));
  p.end();
  const bool ok = w == sizeof(b) && r == sizeof(b) && memcmp(&back, &b, sizeof(b)) == 0;
  memset(&back, 0, sizeof(back));
  return ok;
}

static void stationKeysLoad();
void stationKeysBegin() {
  stationKeysLoad();
  directorKeyLoad();
}

static void stationKeysLoad() {
  blobInit(sBlob);
  sFault = false;
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) {
    sFault = true;
    return;
  }
  if (!p.isKey(kKeysNvs)) {
    p.end();
    return; /* never provisioned: zero keys, no fault */
  }
  static KeyBlob tmp;
  const size_t n = p.getBytes(kKeysNvs, &tmp, sizeof(tmp));
  p.end();
  if (n != sizeof(tmp) || !blobValid(tmp)) {
    memset(&tmp, 0, sizeof(tmp));
    sFault = true;
    return;
  }
  memcpy(&sBlob, &tmp, sizeof(sBlob));
  memset(&tmp, 0, sizeof(tmp));
}

bool stationKeysFault() { return sFault; }
uint8_t stationKeysCount() { return sFault ? 0 : sBlob.count; }

const char *stationKeysId(uint8_t i) {
  if (sFault || i >= sBlob.count) return "";
  return sBlob.e[i].id;
}

int stationKeysLookup(const char *id, uint8_t out[SHOWDUINO_AUTH_KEY_BYTES]) {
  if (out) memset(out, 0, SHOWDUINO_AUTH_KEY_BYTES);
  if (sFault || !id || !out) return -1;
  for (uint8_t i = 0; i < sBlob.count; ++i) {
    if (showduino_emergency_id_equal(sBlob.e[i].id, id)) {
      memcpy(out, sBlob.e[i].key, SHOWDUINO_AUTH_KEY_BYTES);
      return i;
    }
  }
  return -1;
}

bool stationKeysSet(const char *id, const uint8_t key[SHOWDUINO_AUTH_KEY_BYTES]) {
  if (sFault || !showduino_emergency_id_ok(id) || !key) return false;
  int nonzero = 0;
  for (int i = 0; i < SHOWDUINO_AUTH_KEY_BYTES; ++i) nonzero |= key[i];
  if (!nonzero) return false;
  if (directorKeyEquals(key)) return false; /* MISSION 001K: never share with the Director */
  static KeyBlob next;
  memcpy(&next, &sBlob, sizeof(next));
  int slot = -1;
  for (uint8_t i = 0; i < next.count; ++i) {
    if (showduino_emergency_id_equal(next.e[i].id, id)) { slot = i; break; }
  }
  if (slot < 0) {
    if (next.count >= SHOWDUINO_STATION_KEYS_MAX) { memset(&next, 0, sizeof(next)); return false; }
    slot = next.count++;
    memset(&next.e[slot], 0, sizeof(next.e[slot]));
    strncpy(next.e[slot].id, id, sizeof(next.e[slot].id) - 1);
  }
  memcpy(next.e[slot].key, key, SHOWDUINO_AUTH_KEY_BYTES);
  next.crc = blobCrc(next);
  const bool ok = blobSave(next);
  if (ok) memcpy(&sBlob, &next, sizeof(sBlob));
  memset(&next, 0, sizeof(next));
  return ok;
}

bool stationKeysRevoke(const char *id) {
  if (sFault || !id) return false;
  static KeyBlob next;
  memcpy(&next, &sBlob, sizeof(next));
  int slot = -1;
  for (uint8_t i = 0; i < next.count; ++i) {
    if (showduino_emergency_id_equal(next.e[i].id, id)) { slot = i; break; }
  }
  if (slot < 0) { memset(&next, 0, sizeof(next)); return false; }
  for (uint8_t i = (uint8_t)slot; i + 1 < next.count; ++i) next.e[i] = next.e[i + 1];
  next.count--;
  memset(&next.e[next.count], 0, sizeof(next.e[next.count]));
  next.crc = blobCrc(next);
  const bool ok = blobSave(next);
  if (ok) memcpy(&sBlob, &next, sizeof(sBlob));
  memset(&next, 0, sizeof(next));
  return ok;
}

bool stationKeysReset() {
  static KeyBlob empty;
  blobInit(empty);
  if (!blobSave(empty)) return false;
  memcpy(&sBlob, &empty, sizeof(sBlob));
  sFault = false;
  return true;
}

void stationKeysFingerprint(const char *id, char out[9]) {
  out[0] = 0;
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  if (stationKeysLookup(id, key) < 0) return;
  static const char kMsg[] = "SHOWDUINO-KEY-FP|v1";
  uint8_t mac[32];
  if (showduino_auth_hmac_sha256_mbedtls(key, sizeof(key), (const uint8_t *)kMsg,
                                         sizeof(kMsg) - 1, mac) == 0) {
    showduino_auth_hex_encode(mac, 4, out);
  }
  memset(key, 0, sizeof(key));
  memset(mac, 0, sizeof(mac));
}

bool stationCounterLoad(uint32_t *limit) {
  if (!limit) return false;
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  if (!p.isKey(kCtrNvs)) {
    p.end();
    *limit = 1; /* first boot: counters and episodes start at 1 (0 means "none") */
    return true;
  }
  const uint32_t v = p.getUInt(kCtrNvs, 0);
  p.end();
  if (v == 0) return false;
  *limit = v;
  return true;
}

bool stationCounterSave(uint32_t limit) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const size_t w = p.putUInt(kCtrNvs, limit);
  const uint32_t back = p.getUInt(kCtrNvs, 0);
  p.end();
  return w == sizeof(uint32_t) && back == limit;
}

/* ================= MISSION 001K: Director credential ================= */
static const char *kDirNvs = "estop_dkey";
static const char *kDirCtrNvs = "estop_dctr";
static const uint32_t kDirMagic = 0x31594B44u; /* "DKY1" */

struct DirRec {
  uint32_t magic;
  char id[SHOWDUINO_DIR_ID_MAX + 1];
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  uint8_t restricted;
  uint8_t reserved[3];
  uint32_t crc;
};

static DirRec sDir;
static bool sDirPresent = false;
static bool sDirFault = false;
static uint32_t sDirCounter = 0;

static uint32_t dirCrc(const DirRec &r) {
  return crc32(reinterpret_cast<const uint8_t *>(&r), offsetof(DirRec, crc));
}

static void directorKeyLoad() {
  memset(&sDir, 0, sizeof(sDir));
  sDirPresent = false;
  sDirFault = false;
  sDirCounter = 0;
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) { sDirFault = true; return; }
  if (p.isKey(kDirNvs)) {
    static DirRec tmp;
    const size_t n = p.getBytes(kDirNvs, &tmp, sizeof(tmp));
    if (n == sizeof(tmp) && tmp.magic == kDirMagic && tmp.crc == dirCrc(tmp) &&
        showduino_dir_id_ok(tmp.id)) {
      memcpy(&sDir, &tmp, sizeof(sDir));
      sDirPresent = true;
    } else {
      sDirFault = true;
    }
    memset(&tmp, 0, sizeof(tmp));
    if (p.isKey(kDirCtrNvs)) {
      sDirCounter = p.getUInt(kDirCtrNvs, 0xFFFFFFFFu);
      if (sDirCounter == 0xFFFFFFFFu) sDirFault = true; /* unreadable: refuse everything */
    }
  }
  p.end();
}

static bool directorKeyEquals(const uint8_t *key) {
  return sDirPresent && key && showduino_auth_ct_equal(sDir.key, key, SHOWDUINO_AUTH_KEY_BYTES);
}

bool directorKeyFault() { return sDirFault; }

void directorKeyGet(ShowduinoDirKeyRec *rec) {
  if (!rec) return;
  memset(rec, 0, sizeof(*rec));
  if (!sDirPresent || sDirFault) return;
  rec->present = 1;
  memcpy(rec->id, sDir.id, sizeof(rec->id));
  memcpy(rec->key, sDir.key, sizeof(rec->key));
  rec->restricted = sDir.restricted;
  rec->lastCounter = sDirCounter;
}

static bool dirSave(const DirRec &r) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const size_t w = p.putBytes(kDirNvs, &r, sizeof(r));
  static DirRec back;
  const size_t n = p.getBytes(kDirNvs, &back, sizeof(back));
  p.end();
  const bool ok = w == sizeof(r) && n == sizeof(r) && memcmp(&back, &r, sizeof(r)) == 0;
  memset(&back, 0, sizeof(back));
  return ok;
}

bool directorCounterSave(uint32_t value) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const size_t w = p.putUInt(kDirCtrNvs, value);
  const uint32_t back = p.getUInt(kDirCtrNvs, 0xFFFFFFFFu);
  p.end();
  if (w != sizeof(uint32_t) || back != value) return false;
  sDirCounter = value;
  return true;
}

bool directorKeySet(const char *id, const uint8_t key[SHOWDUINO_AUTH_KEY_BYTES], bool restricted) {
  if (sDirFault || !showduino_dir_id_ok(id) || !key) return false;
  int nonzero = 0;
  for (int i = 0; i < SHOWDUINO_AUTH_KEY_BYTES; ++i) nonzero |= key[i];
  if (!nonzero) return false;
  for (uint8_t i = 0; i < stationKeysCount(); ++i) {
    uint8_t sk[SHOWDUINO_AUTH_KEY_BYTES];
    const bool same = stationKeysLookup(stationKeysId(i), sk) >= 0 &&
                      showduino_auth_ct_equal(sk, key, SHOWDUINO_AUTH_KEY_BYTES);
    memset(sk, 0, sizeof(sk));
    if (same) return false;
  }
  const bool sameKey = sDirPresent && showduino_auth_ct_equal(sDir.key, key, SHOWDUINO_AUTH_KEY_BYTES) &&
                       strcmp(sDir.id, id) == 0;
  static DirRec next;
  memset(&next, 0, sizeof(next));
  next.magic = kDirMagic;
  strncpy(next.id, id, sizeof(next.id) - 1);
  memcpy(next.key, key, SHOWDUINO_AUTH_KEY_BYTES);
  next.restricted = restricted ? 1 : (sameKey ? sDir.restricted : 0);
  next.crc = dirCrc(next);
  /* Record first, then (for a different key) reset the counter. If the record write
   * fails the old key and its counter are untouched. If the counter reset fails the
   * new key keeps the old, higher counter: its first messages are rejected as
   * replays until the Director's counter passes it (fail closed, never a replay). */
  const bool ok = dirSave(next);
  if (ok) { memcpy(&sDir, &next, sizeof(sDir)); sDirPresent = true; }
  memset(&next, 0, sizeof(next));
  if (!ok) return false;
  return sameKey || directorCounterSave(0);
}

bool directorKeyRevoke() {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  p.remove(kDirNvs);
  const bool ok = !p.isKey(kDirNvs);
  p.end();
  if (ok) { memset(&sDir, 0, sizeof(sDir)); sDirPresent = false; }
  return ok;
}

bool directorKeyReset() {
  if (!directorKeyRevoke()) return false;
  sDirFault = false;
  return directorCounterSave(0);
}

bool directorKeyClearRestriction() {
  if (!sDirPresent || !sDir.restricted) return true;
  static DirRec next;
  memcpy(&next, &sDir, sizeof(next));
  next.restricted = 0;
  next.crc = dirCrc(next);
  const bool ok = dirSave(next);
  if (ok) memcpy(&sDir, &next, sizeof(sDir));
  memset(&next, 0, sizeof(next));
  return ok;
}

void directorKeyFingerprint(char out[9]) {
  out[0] = 0;
  if (!sDirPresent) return;
  static const char kMsg[] = "SHOWDUINO-KEY-FP|v1";
  uint8_t mac[32];
  if (showduino_auth_hmac_sha256_mbedtls(sDir.key, sizeof(sDir.key), (const uint8_t *)kMsg,
                                         sizeof(kMsg) - 1, mac) == 0) {
    showduino_auth_hex_encode(mac, 4, out);
  }
  memset(mac, 0, sizeof(mac));
}
