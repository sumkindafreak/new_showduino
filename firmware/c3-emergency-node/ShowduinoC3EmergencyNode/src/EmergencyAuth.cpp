#include "EmergencyAuth.h"
#include "EmergencyIdentity.h"
#include "EmergencyInput.h"
#include "EmergencyProtocol.h"
#include "EspNowEmergencyTransport.h"
#include "EstopPixelProtocol.h"
#include <Preferences.h>
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_station_auth.h"
#include "../../../protocol/showduino_station_auth_esp32.h"
#include "../../../protocol/showduino_station_responder.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_node_packet.h"

/* NVS namespace "estop_auth":
 *   kblob : KeyRec { magic, key[32], crc32 }
 *   ctr   : uint32 last accepted command counter */
static const char *kNs = "estop_auth";
static const uint32_t kMagic = 0x31594B45u; /* "EKY1" */

struct KeyRec {
  uint32_t magic;
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  uint32_t crc;
};

static ShowduinoStationAuthState sSt;
static bool sStoreFault = false;
static uint32_t sRejectLogMs = 0;

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

static int persistCounterCb(void *ctx, uint32_t value) {
  (void)ctx;
  Preferences p;
  if (!p.begin(kNs, false)) return 0;
  const size_t w = p.putUInt("ctr", value);
  const uint32_t back = p.getUInt("ctr", 0);
  p.end();
  return (w == sizeof(uint32_t) && back == value) ? 1 : 0;
}

static bool saveKey(const uint8_t *key) {
  KeyRec r;
  memset(&r, 0, sizeof(r));
  r.magic = kMagic;
  if (key) memcpy(r.key, key, sizeof(r.key));
  r.crc = recCrc(r);
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  bool ok;
  if (key) {
    const size_t w = p.putBytes("kblob", &r, sizeof(r));
    KeyRec back;
    const size_t n = p.getBytes("kblob", &back, sizeof(back));
    ok = w == sizeof(r) && n == sizeof(r) && memcmp(&back, &r, sizeof(r)) == 0;
    memset(&back, 0, sizeof(back));
  } else {
    p.remove("kblob");
    ok = !p.isKey("kblob");
  }
  p.end();
  memset(&r, 0, sizeof(r));
  return ok;
}

void emergencyAuthBegin() {
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  memset(key, 0, sizeof(key));
  bool valid = false;
  uint32_t ctr = 0;
  sStoreFault = false;
  Preferences p;
  if (p.begin(kNs, false)) {
    if (p.isKey("kblob")) {
      KeyRec r;
      const size_t n = p.getBytes("kblob", &r, sizeof(r));
      if (n == sizeof(r) && r.magic == kMagic && r.crc == recCrc(r)) {
        memcpy(key, r.key, sizeof(key));
        valid = true;
      } else {
        sStoreFault = true;
      }
      memset(&r, 0, sizeof(r));
    }
    ctr = p.getUInt("ctr", 0);
    p.end();
  } else {
    sStoreFault = true;
  }
  showduino_responder_boot(&sSt, key, valid ? 1 : 0, ctr, gEmergencyMachine.latched);
  memset(key, 0, sizeof(key));
  if (sStoreFault) {
    Serial.println("[ESTOP-AUTH] key record unreadable — station cannot authenticate; PROVISION again");
  } else if (!valid) {
    Serial.println("[ESTOP-AUTH] NOT PROVISIONED — the P4 will not accept this station's release");
  } else {
    Serial.printf("[ESTOP-AUTH] provisioned as %s (last cmd counter %lu)\n", emergencyIdentityId(),
                  (unsigned long)ctr);
  }
}

bool emergencyAuthProvisioned() { return sSt.provisioned != 0; }

void emergencyAuthService() {
  showduino_responder_track_latch(&sSt, gEmergencyMachine.latched);
}

static void fingerprint(char out[9]) {
  out[0] = 0;
  if (!sSt.provisioned) return;
  static const char kMsg[] = "SHOWDUINO-KEY-FP|v1";
  uint8_t mac[32];
  if (showduino_auth_hmac_sha256_mbedtls(sSt.key, sizeof(sSt.key), (const uint8_t *)kMsg,
                                         sizeof(kMsg) - 1, mac) == 0) {
    showduino_auth_hex_encode(mac, 4, out);
  }
  memset(mac, 0, sizeof(mac));
}

static bool provisioningAllowed() {
  if (gEmergencyMachine.latched || emergencyInputPressed()) {
    Serial.println("[ESTOP-AUTH] REFUSED — release the button and clear the emergency first");
    return false;
  }
  return true;
}

static bool installKey(const char *id, const uint8_t *key) {
  if (!emergencyIdentitySetId(id)) {
    Serial.println("[ESTOP-AUTH] REFUSED — ID must be ESTOP-<1..4 digits>");
    return false;
  }
  if (!saveKey(key)) {
    Serial.println("[ESTOP-AUTH] FAILED — key not stored (verify failed)");
    return false;
  }
  /* A different key starts a new command history (no command was ever signed with
   * it). Re-entering the SAME key keeps the counter, so commands captured under it
   * stay rejected. Never reuse a retired key: see the provisioning guide. */
  const bool sameKey = sSt.provisioned &&
                       showduino_auth_ct_equal(sSt.key, key, SHOWDUINO_AUTH_KEY_BYTES);
  const uint32_t keep = sameKey ? sSt.lastCmdCounter : 0u;
  if (!sameKey && !persistCounterCb(nullptr, 0)) {
    Serial.println("[ESTOP-AUTH] FAILED — counter not stored");
    return false;
  }
  showduino_responder_boot(&sSt, key, 1, keep, gEmergencyMachine.latched);
  sStoreFault = false;
  return true;
}

bool emergencyAuthHandleUsb(const char *line) {
  if (!line || strncmp(line, "PROVISION", 9) != 0) return false;
  const char *a = line + 9;
  while (*a == ' ') ++a;

  if (!strncmp(a, "STATUS", 6)) {
    char fp[9];
    fingerprint(fp);
    Serial.printf("[ESTOP-AUTH] id=%s provisioned=%s fp=%s storeFault=%u lastCmd=%lu\n",
                  emergencyIdentityId(), sSt.provisioned ? "yes" : "no", fp[0] ? fp : "-",
                  sStoreFault ? 1U : 0U, (unsigned long)sSt.lastCmdCounter);
    return true;
  }
  if (!provisioningAllowed()) return true;

  if (!strncmp(a, "CLEAR", 5)) {
    const bool ok = saveKey(nullptr);
    if (ok) showduino_responder_boot(&sSt, nullptr, 0, sSt.lastCmdCounter, gEmergencyMachine.latched);
    Serial.printf("[ESTOP-AUTH] key %s — revoke it on the P4 too (EMERGENCY:KEY:REVOKE:%s)\n",
                  ok ? "CLEARED" : "CLEAR FAILED", emergencyIdentityId());
    return true;
  }

  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  if (!strncmp(a, "NEW ", 4)) {
    strncpy(id, a + 4, sizeof(id) - 1);
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    showduino_auth_rand_esp32(key, sizeof(key)); /* true RNG while Wi-Fi is running */
    int nonzero = 0;
    for (size_t i = 0; i < sizeof(key); ++i) nonzero |= key[i];
    const bool ok = nonzero && installKey(id, key);
    if (ok) {
      char hex[SHOWDUINO_AUTH_KEY_HEX + 1];
      showduino_auth_hex_encode(key, sizeof(key), hex);
      char fp[9];
      fingerprint(fp);
      /* Printed once, to USB only (never to the SD log). */
      Serial.printf("[ESTOP-AUTH] NEW KEY for %s fp=%s\n", emergencyIdentityId(), fp);
      Serial.printf("[ESTOP-AUTH] On the P4 USB console enter:\nEMERGENCY:KEY:SET:%s:%s\n",
                    emergencyIdentityId(), hex);
      memset(hex, 0, sizeof(hex));
    }
    memset(key, 0, sizeof(key));
    return true;
  }
  if (!strncmp(a, "SET ", 4)) {
    const char *p = a + 4;
    const char *sp = strchr(p, ' ');
    if (!sp || (size_t)(sp - p) > SHOWDUINO_EMERGENCY_ID_MAX || strlen(sp + 1) != SHOWDUINO_AUTH_KEY_HEX) {
      Serial.println("[ESTOP-AUTH] usage: PROVISION SET ESTOP-nn <64 hex chars>");
      return true;
    }
    memcpy(id, p, (size_t)(sp - p));
    id[sp - p] = 0;
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    const bool parsed = showduino_auth_hex_decode(sp + 1, sizeof(key), key) != 0;
    const bool ok = parsed && installKey(id, key);
    memset(key, 0, sizeof(key));
    char fp[9];
    fingerprint(fp);
    Serial.printf("[ESTOP-AUTH] SET %s %s fp=%s\n", id, ok ? "OK" : "FAILED", ok ? fp : "-");
    return true;
  }
  Serial.println("[ESTOP-AUTH] PROVISION NEW ESTOP-nn | PROVISION SET ESTOP-nn <hex64> | "
                 "PROVISION CLEAR | PROVISION STATUS");
  return true;
}

bool emergencyAuthHandleRadio(const char *line, uint32_t sequence) {
  if (!line) return false;
  showduino_responder_track_latch(&sSt, gEmergencyMachine.latched);

  if (!strncmp(line, "CHAL:", 5)) {
    char out[SHOWDUINO_NODE_COMMAND_MAX];
    if (showduino_responder_answer_chal(&sSt, emergencyIdentityId(),
                                        emergencyInputPressed() ? 1 : 0,
                                        gEmergencyMachine.latched, line,
                                        showduino_auth_hmac_sha256_mbedtls, out, sizeof(out))) {
      emergencyEspNowSend(out, sequence);
    }
    return true;
  }

  if (!strncmp(line, "CMD:", 4)) {
    uint32_t episode = 0;
    const int r = showduino_responder_accept_cmd(&sSt, emergencyIdentityId(),
                                                 gEmergencyMachine.latched, line,
                                                 showduino_auth_hmac_sha256_mbedtls,
                                                 persistCounterCb, nullptr, &episode);
    if (r == SHOWDUINO_RESP_ACK) {
      showduino_emergency_machine_ack(&gEmergencyMachine);
      SD_LOGI("ESTOP", "P4 ACK (authenticated) episode=%lu — not a clear", (unsigned long)episode);
    } else if (r == SHOWDUINO_RESP_GCL) {
      emergencyProtocolOnGlobalClear();
      showduino_responder_track_latch(&sSt, gEmergencyMachine.latched);
    } else if (r != SHOWDUINO_RESP_REJECT_IDENTITY && r != SHOWDUINO_RESP_REJECT_REPLAY) {
      /* Other stations' commands and broadcast duplicates are normal; others are not. */
      if (showduino_log_rate_ok(&sRejectLogMs, millis(), 5000UL)) {
        SD_LOGW("ESTOP", "CMD rejected (%d)", r);
      }
    }
    return true;
  }
  return false;
}
