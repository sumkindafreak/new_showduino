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

void stationKeysBegin() {
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
