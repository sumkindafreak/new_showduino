#include "EmergencyNodeLink.h"
#include "showduino_station_model.h"
#include "../../../protocol/showduino_legacy_strings.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_emergency_latch.h"
#include "../EmergencyStationStore.h"
#include "../../../protocol/showduino_node_packet.h"
#include "../../../protocol/showduino_station_auth.h"
#include "../../../protocol/showduino_station_auth_esp32.h"
#include "EmergencyStationKeys.h"

extern bool emergencyLocked;
extern uint8_t gEmergencySourceId;
void triggerEmergencyWireless();
void stageCommsSendLine(const char *line);
bool sendToDirector(const String &message);

/* Wireless emergency source id used by the P4 latch. Must match .ino. */
#ifndef SHOWDUINO_EMERGENCY_SOURCE_WIRELESS
#define SHOWDUINO_EMERGENCY_SOURCE_WIRELESS 4
#endif

static EmergencyNodeStatus sNodes[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
static ShowduinoEmergencyAuthority sAuth;
static uint32_t sSeq = 1;
static uint32_t sKeepaliveMs = 0;
static uint32_t sPublishMs = 0;
static uint32_t sDupLogMs = 0;
static char sLastWire[24] = "";
static char sLastDetail[96] = "";
static char sLastSafety[32] = "";
static char sLastSource[64] = "";

/* ---- MISSION 001J: station authentication state (P4 only; the S3 holds none) ---- */
static ShowduinoAuthChalTable sChal;
static ShowduinoAuthCounter sCtr;
static bool sCounterLoaded = false;
static bool sCounterFault = false;
static uint32_t sEpisode = 0;           /* current episode; 0 when idle */
static uint32_t sEpisodeRetryMs = 0;
static ShowduinoNonceGen sNonce;
static uint32_t sChalMs = 0;
static uint32_t sClearRequestSeq = 0;
static bool sTableOverflow = false;
static uint32_t sAuthRejectLogMs = 0;
/* GCL commands reserved by a successful confirm, sent by emergencyNodeLinkOnEmergency(false). */
static char sGclIds[SHOWDUINO_EMERGENCY_NODE_MAX_NODES][SHOWDUINO_EMERGENCY_ID_MAX + 1];
static uint32_t sGclCounters[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
static uint8_t sGclCount = 0;
static uint32_t sGclEpisode = 0;
static bool sGclReady = false;

static int persistCounterCb(void *ctx, uint32_t value) {
  (void)ctx;
  return stationCounterSave(value) ? 1 : 0;
}

/* Hands out a counter value. A failed block write sets sCounterFault; a later
 * successful one clears it. Nothing is ever issued with an unpersisted counter. */
static bool nextCounter(uint32_t *out) {
  if (!sCounterLoaded ||
      !showduino_auth_counter_next(&sCtr, persistCounterCb, nullptr, out)) {
    sCounterFault = true;
    return false;
  }
  sCounterFault = false;
  return true;
}

static int keyForCb(void *ctx, const char *id, uint8_t key[SHOWDUINO_AUTH_KEY_BYTES]) {
  (void)ctx;
  return stationKeysLookup(id, key);
}

static int nonceRandCb(uint8_t *out, size_t n) {
  return showduino_nonce_gen_next(&sNonce, out, n);
}

static bool isProvisioned(const char *id) {
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  const bool ok = stationKeysLookup(id, key) >= 0;
  memset(key, 0, sizeof(key));
  return ok;
}

static void routeTo(const char *id, uint32_t seq, const char *cmd) {
  char line[180];
  snprintf(line, sizeof(line), "ROUTE:EMERGENCY:%s:%lu:%s",
           id && id[0] ? id : SHOWDUINO_EMERGENCY_ID_DEFAULT,
           (unsigned long)seq,
           cmd ? cmd : "");
  stageCommsSendLine(line);
}

/* Sends an authenticated command to one station: unicast through the relay's MAC
 * table and a broadcast copy (a poisoned MAC table cannot divert it; the station
 * rejects the duplicate by its counter). */
static bool sendCmd(const char *id, const char *op, uint32_t episode, const uint32_t *fixedCounter) {
  if (!id || !id[0] || episode == 0) return false;
  uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
  if (stationKeysLookup(id, key) < 0) return false;
  uint32_t counter = 0;
  if (fixedCounter) {
    counter = *fixedCounter;
  } else if (!nextCounter(&counter)) {
    memset(key, 0, sizeof(key));
    return false;
  }
  char line[SHOWDUINO_NODE_COMMAND_MAX];
  const int ok = showduino_auth_format_cmd(id, counter, op, episode, key, sizeof(key),
                                           showduino_auth_hmac_sha256_mbedtls, line, sizeof(line));
  memset(key, 0, sizeof(key));
  if (!ok) return false;
  routeTo(id, sSeq++, line);
  routeTo(SHOWDUINO_EMERGENCY_ROUTE_ALL, sSeq++, line);
  return true;
}

static bool issueChallenge() {
  if (!sNonce.seeded) return false;
  char nonce[SHOWDUINO_AUTH_NONCE_HEX + 1];
  if (!showduino_auth_chal_issue(&sChal, millis(), sEpisode, nonceRandCb, nonce)) return false;
  char line[SHOWDUINO_NODE_COMMAND_MAX];
  if (!showduino_auth_format_chal(sEpisode, nonce, line, sizeof(line))) return false;
  routeTo(SHOWDUINO_EMERGENCY_ROUTE_ALL, sSeq++, line);
  sChalMs = millis();
  return true;
}

/* While latched, the episode id must be durable before any ACK/GCL can name it.
 * Storage runs after actuation (MISSION 001F) and is retried once a second. */
static void ensureEpisode() {
  if (!emergencyLocked || sEpisode) return;
  if (sEpisodeRetryMs && (millis() - sEpisodeRetryMs) < 1000UL) return;
  uint32_t e = 0;
  if (!nextCounter(&e)) {
    sEpisodeRetryMs = millis();
    SD_LOGW("ESTOP", "episode id not stored — station commands held, clear blocked");
    return;
  }
  sEpisodeRetryMs = 0;
  sEpisode = e;
  showduino_auth_chal_reset(&sChal);
  SD_LOGI("ESTOP", "EMERGENCY EPISODE %lu", (unsigned long)sEpisode);
}

static EmergencyNodeStatus *findById(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && showduino_emergency_id_equal(sNodes[i].id, id)) {
      return &sNodes[i];
    }
  }
  return nullptr;
}

static EmergencyNodeStatus *findByMac(const char *mac) {
  if (!mac || strlen(mac) < 17) return nullptr;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && !strcasecmp(sNodes[i].mac, mac)) return &sNodes[i];
  }
  return nullptr;
}

/* ---- Clear-blocker persistence (MISSION 001D) ----
 * Only stations whose last valid report says pressed are stored. Stored only on
 * a change, never per announce. A failed or unverified write sets sPersistDirty,
 * and clear stays blocked until a verified write succeeds. While the boot-time
 * record is unknown, nothing is written, so an unknown record is never replaced
 * by an empty one. */
static bool sBooted = false;
static bool sUnknownBootState = false;
static ShowduinoBlocker sStaged[SHOWDUINO_BLOCKER_MAX];
static uint8_t sStagedCount = 0;
static ShowduinoBlocker sPersisted[SHOWDUINO_BLOCKER_MAX];
static uint8_t sPersistedCount = 0;
static bool sPersistedValid = false;
static bool sPersistDirty = false;

static void loadStationModel(ShowduinoStationModel *t, bool *prevOnline, bool *prevOpen);

/* MISSION 001I: the stored set is every station the clear depends on (asserted in
 * this episode, or not yet verified, or offline, or pressed). Built by the same
 * shared rule the clear gate uses. Returns false if it cannot be built faithfully. */
static bool snapshotRequired(ShowduinoBlocker *out, uint8_t *n) {
  ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
  loadStationModel(t, nullptr, nullptr);
  memset(out, 0, sizeof(ShowduinoBlocker) * SHOWDUINO_BLOCKER_MAX);
  int count = 0;
  if (!showduino_station_model_collect_required(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES,
                                                out, SHOWDUINO_BLOCKER_MAX, &count)) {
    *n = 0;
    return false;
  }
  *n = (uint8_t)count;
  return true;
}

bool emergencyNodeLinkPersistBlockers() {
  if (sUnknownBootState) return false; /* never overwrite an unknown record */
  ShowduinoBlocker snap[SHOWDUINO_BLOCKER_MAX];
  uint8_t n = 0;
  if (!snapshotRequired(snap, &n)) {
    sPersistDirty = true;
    SD_LOGW("ESTOP", "station blocker set cannot be built — emergency clear stays blocked");
    return false;
  }
  const int equal = showduino_blocker_sets_equal(snap, n, sPersisted, sPersistedCount);
  if (!showduino_station_persist_needed(sPersistedValid ? 1 : 0, sPersistDirty ? 1 : 0, equal)) {
    return true;
  }
  char enc[SHOWDUINO_BLOCKER_BUF];
  if (showduino_blocker_encode(snap, n, enc, sizeof(enc)) < 0 ||
      !emergencyStationStoreSave(enc)) {
    sPersistDirty = true;
    SD_LOGW("ESTOP", "station blocker set not stored — emergency clear stays blocked");
    return false;
  }
  memcpy(sPersisted, snap, sizeof(snap));
  sPersistedCount = n;
  sPersistedValid = true;
  sPersistDirty = false;
  return true;
}

/* MISSION 001I: an accepted Director clear ends the global-latch episode. Each
 * station is then required only while it is still pressed, unverified or offline.
 * Called after the latch clear is stored. */
void emergencyNodeLinkEndEpisode() {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    sNodes[i].asserted = false;
  }
  emergencyNodeLinkPersistBlockers();
}

void emergencyNodeLinkRetryBlockers() {
  if (sPersistDirty && !sUnknownBootState) emergencyNodeLinkPersistBlockers();
}

bool emergencyNodeLinkBlockerStorageFault() { return sPersistDirty || sUnknownBootState; }

int emergencyNodeLinkClearBlocked(bool confirm) {
  ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
  loadStationModel(t, nullptr, nullptr);
  char ids[SHOWDUINO_EMERGENCY_NODE_MAX_NODES][SHOWDUINO_BLOCKER_ID_CHARS + 1];
  memset(ids, 0, sizeof(ids));
  const uint8_t keyCount = stationKeysCount();
  for (uint8_t i = 0; i < keyCount && i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    showduino_station_copy(ids[i], sizeof(ids[i]), stationKeysId(i));
  }
  /* No durable episode while provisioned stations exist: no GCL can be issued. */
  const bool counterBad = sCounterFault || !sCounterLoaded || stationKeysFault() ||
                          (emergencyLocked && keyCount > 0 && sEpisode == 0);
  const uint32_t since = confirm ? showduino_station_proof_since(sClearRequestSeq, sChal.lastSeq) : 0;
  const int decision = showduino_station_clear_decision(
      t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, ids, confirm ? keyCount : 0, since,
      sUnknownBootState ? 1 : 0, sPersistDirty ? 1 : 0, counterBad ? 1 : 0,
      sTableOverflow ? 1 : 0, sBooted ? 1 : 0);
  if (decision != SHOWDUINO_CLEAR_OK || !confirm) return decision;
  /* Reserve one GCL counter per provisioned station before anything changes. */
  sGclReady = false;
  for (uint8_t i = 0; i < keyCount; ++i) {
    if (!nextCounter(&sGclCounters[i])) return SHOWDUINO_CLEAR_BLOCK_STORAGE;
    showduino_station_copy(sGclIds[i], sizeof(sGclIds[i]), ids[i]);
  }
  sGclCount = keyCount;
  sGclEpisode = sEpisode;
  sGclReady = keyCount > 0;
  return SHOWDUINO_CLEAR_OK;
}

const char *emergencyNodeLinkClearReason(int decision) {
  switch (decision) {
    case SHOWDUINO_CLEAR_OK: return "OK";
    case SHOWDUINO_CLEAR_BLOCK_STATION: return "WIRELESS_ASSERTED";
    case SHOWDUINO_CLEAR_BLOCK_STORAGE: return "STATION_STORAGE";
    case SHOWDUINO_CLEAR_BLOCK_PROOF: return "STATION_UNVERIFIED";
    case SHOWDUINO_CLEAR_BLOCK_OVERFLOW: return "STATION_OVERFLOW";
    default: return "WIRELESS_BLOCKED";
  }
}

void emergencyNodeLinkOnClearRequest() {
  sClearRequestSeq = showduino_auth_chal_next_seq(&sChal);
  (void)issueChallenge();
}

void emergencyNodeLinkSeedNonce(const uint8_t seed[32]) {
  if (!showduino_nonce_gen_seed(&sNonce, seed, showduino_auth_hmac_sha256_mbedtls)) {
    SD_LOGE("ESTOP", "station nonce seed rejected — stations cannot be verified, clear blocked");
  }
}

bool emergencyNodeLinkBlockersVerified() { return !sPersistDirty && !sUnknownBootState; }

void emergencyNodeLinkStageRestore(const ShowduinoBlocker *list, uint8_t n,
                                   ShowduinoBlockerStatus status, bool latched,
                                   bool latchUnknown) {
  sStagedCount = 0;
  sPersistedValid = false;
  /* Latched with an unreadable or incomplete record, or a latch byte that was
   * written unverified or cannot be read: clear stays refused. */
  sUnknownBootState = showduino_boot_state_unknown(latched ? 1 : 0, (int)status,
                                                   latchUnknown ? 1 : 0) != 0;
  if (sUnknownBootState) {
    SD_LOGE("ESTOP", "station record unknown after restart — clear blocked, service recovery required");
    return;
  }
  if (latched && status == SHOWDUINO_BLOCKER_OK && list && n <= SHOWDUINO_BLOCKER_MAX) {
    memcpy(sStaged, list, sizeof(ShowduinoBlocker) * n);
    sStagedCount = n;
    memcpy(sPersisted, list, sizeof(ShowduinoBlocker) * n);
    sPersistedCount = n;
    sPersistedValid = true;
  }
}

static EmergencyNodeStatus *allocSlot(const char *id, const char *mac) {
  EmergencyNodeStatus *hit = findById(id);
  if (!hit) hit = findByMac(mac);
  if (hit) return hit;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used) {
      sNodes[i] = EmergencyNodeStatus();
      sNodes[i].used = true;
      return &sNodes[i];
    }
  }
  return nullptr;
}

static void publishSafety() {
  const bool fault = emergencyNodeLinkSafetyFault();
  showduino_emergency_authority_offline(&sAuth, fault ? 1 : 0);
  char line[40];
  snprintf(line, sizeof(line), "STATE:SAFETY:%s",
           fault ? SHOWDUINO_WIRE_SAFETY_ESTOP_FAULT : SHOWDUINO_WIRE_SAFETY_ESTOP_OK);
  if (strcmp(sLastSafety, line) != 0) {
    if (sendToDirector(String(line))) {
      strncpy(sLastSafety, line, sizeof(sLastSafety) - 1);
    }
    if (fault) {
      SD_LOGW("ESTOP", "SAFETY WARNING — Emergency Node offline");
    }
  }
}

static void publishSource() {
  if (!emergencyLocked) {
    if (sLastSource[0]) sLastSource[0] = 0;
    return;
  }
  char line[80];
  if (gEmergencySourceId == 2) {
    snprintf(line, sizeof(line), "%sHARDWIRED",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else if (gEmergencySourceId == SHOWDUINO_EMERGENCY_SOURCE_WIRELESS &&
             sAuth.primary_id[0]) {
    snprintf(line, sizeof(line), "%sWIRELESS:%s:%s",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX,
             sAuth.primary_id,
             sAuth.primary_name[0] ? sAuth.primary_name : "-");
  } else if (gEmergencySourceId == 3) {
    snprintf(line, sizeof(line), "%sUSB",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else if (gEmergencySourceId == 1) {
    snprintf(line, sizeof(line), "%sREMOTE",
             SHOWDUINO_WIRE_STATE_EMERGENCY_SOURCE_PREFIX);
  } else {
    return;
  }
  if (strcmp(sLastSource, line) != 0 && sendToDirector(String(line))) {
    strncpy(sLastSource, line, sizeof(sLastSource) - 1);
  }
}

/* Adapter between the firmware station table and the shared P4 model. */
static void loadStationModel(ShowduinoStationModel *t, bool *prevOnline, bool *prevOpen) {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    memset(&t[i], 0, sizeof(t[i]));
    t[i].used = sNodes[i].used ? 1 : 0;
    t[i].online = sNodes[i].online ? 1 : 0;
    t[i].reportKnown = sNodes[i].reportKnown ? 1 : 0;
    t[i].inputOpen = sNodes[i].inputOpen ? 1 : 0;
    t[i].announcedLatched = sNodes[i].announcedLatched ? 1 : 0;
    t[i].asserted = sNodes[i].asserted ? 1 : 0;
    t[i].ackedReport = sNodes[i].ackedReport ? 1 : 0;
    t[i].proofChalSeq = sNodes[i].proofChalSeq;
    showduino_station_copy(t[i].id, sizeof(t[i].id), sNodes[i].id);
    showduino_station_copy(t[i].mac, sizeof(t[i].mac), sNodes[i].mac);
    if (prevOnline) prevOnline[i] = sNodes[i].online;
    if (prevOpen) prevOpen[i] = sNodes[i].inputOpen;
  }
}

static void storeStationModel(const ShowduinoStationModel *t) {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    sNodes[i].used = t[i].used ? true : false;
    sNodes[i].online = t[i].online ? true : false;
    sNodes[i].reportKnown = t[i].reportKnown ? true : false;
    sNodes[i].inputOpen = t[i].inputOpen ? true : false;
    sNodes[i].announcedLatched = t[i].announcedLatched ? true : false;
    sNodes[i].asserted = t[i].asserted ? true : false;
    sNodes[i].ackedReport = t[i].ackedReport ? true : false;
    sNodes[i].proofChalSeq = t[i].proofChalSeq;
    showduino_station_copy(sNodes[i].id, sizeof(sNodes[i].id), t[i].id);
    showduino_station_copy(sNodes[i].mac, sizeof(sNodes[i].mac), t[i].mac);
  }
}

uint8_t emergencyNodeLinkPressedCount() {
  /* Stations that block clear: PRESSED, no verified report this session, or OFFLINE. */
  ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
  loadStationModel(t, nullptr, nullptr);
  return (uint8_t)showduino_station_model_count_blocking(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
}

static const char *wireToken() {
  uint8_t online = 0, seen = 0, asserting = 0, offline = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].online) online++;
    else offline++;
    if (sNodes[i].latched) asserting++;
  }
  if (asserting) return "ACTIVE";
  if (offline) return "FAULT";
  if (!seen || !online) return "OFFLINE";
  return "ONLINE";
}

static void publishState(bool force = false) {
  char line[48];
  const char *tok = wireToken();
  if (force || strcmp(sLastWire, tok) != 0) {
    snprintf(line, sizeof(line), "%s%s", SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_PREFIX, tok);
    if (sendToDirector(String(line))) {
      strncpy(sLastWire, tok, sizeof(sLastWire) - 1);
    }
  }

  const char *firstId = "-";
  const char *firstName = "-";
  const char *firstSt = "OFFLINE";
  uint8_t online = 0, seen = 0, asserting = 0, offline = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    seen++;
    if (sNodes[i].latched) asserting++;
    if (sNodes[i].online) {
      if (!online) {
        firstId = sNodes[i].id[0] ? sNodes[i].id : "-";
        firstName = sNodes[i].name[0] ? sNodes[i].name : "-";
        firstSt = sNodes[i].state[0] ? sNodes[i].state : "ONLINE";
      }
      online++;
    } else {
      offline++;
    }
  }
  char detail[96];
  snprintf(detail, sizeof(detail), "%s%u:%u:%u:%u:%s:%s:%s",
           SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_DETAIL_PREFIX,
           (unsigned)online, (unsigned)seen, (unsigned)asserting, (unsigned)offline,
           firstId, firstName, firstSt);
  if (force || strcmp(sLastDetail, detail) != 0) {
    if (sendToDirector(String(detail))) {
      strncpy(sLastDetail, detail, sizeof(sLastDetail) - 1);
    }
  }

  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (!sNodes[i].used || !sNodes[i].seen) continue;
    char stline[96];
    snprintf(stline, sizeof(stline), "%s%u:%s:%s:%u:%u:%u:%u:%s",
             SHOWDUINO_WIRE_STATE_NODE_EMERGENCY_STATION_PREFIX,
             (unsigned)i,
             sNodes[i].id[0] ? sNodes[i].id : "-",
             sNodes[i].name[0] ? sNodes[i].name : "-",
             sNodes[i].online ? 1U : 0U,
             sNodes[i].inputOpen ? 1U : 0U,
             sNodes[i].latched ? 1U : 0U,
             sNodes[i].acked ? 1U : 0U,
             sNodes[i].state[0] ? sNodes[i].state : "OFFLINE");
    sendToDirector(String(stline));
  }
  publishSafety();
  publishSource();
  sPublishMs = millis();
}

void emergencyNodeLinkPublishToDirector() { publishState(true); }

uint8_t emergencyNodeLinkOnlineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].online) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkSeenCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkOfflineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].seen && !sNodes[i].online) n++;
  }
  return n;
}

uint8_t emergencyNodeLinkAssertingCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    if (sNodes[i].used && sNodes[i].latched) n++;
  }
  return n;
}

bool emergencyNodeLinkSafetyFault() {
  return emergencyNodeLinkOfflineCount() > 0;
}

const EmergencyNodeStatus *emergencyNodeLinkFind(const char *id) { return findById(id); }

const EmergencyNodeStatus *emergencyNodeLinkSlot(uint8_t i) {
  if (i >= SHOWDUINO_EMERGENCY_NODE_MAX_NODES) return nullptr;
  if (!sNodes[i].used) return nullptr;
  return &sNodes[i];
}

void emergencyNodeLinkPrimarySource(char *kind, size_t kindn,
                                    char *id, size_t idn,
                                    char *name, size_t namen) {
  if (kind && kindn) {
    strncpy(kind, sAuth.primary_kind[0] ? sAuth.primary_kind : "", kindn - 1);
    kind[kindn - 1] = 0;
  }
  if (id && idn) {
    strncpy(id, sAuth.primary_id, idn - 1);
    id[idn - 1] = 0;
  }
  if (name && namen) {
    strncpy(name, sAuth.primary_name, namen - 1);
    name[namen - 1] = 0;
  }
}

/* MISSION 001H: the P4 global latch request. Never changes the button state.
 * Button state is decided by showduino_station_model (ANNOUNCE or ASSERT rules). */
static void ackIfDue(EmergencyNodeStatus *st) {
  if (!st || !st->id[0] || !emergencyLocked || !sEpisode) return;
  const uint32_t now = millis();
  if (st->ackMs && (now - st->ackMs) < 1000UL) return;
  if (!isProvisioned(st->id)) return;
  if (sendCmd(st->id, "ACK", sEpisode, nullptr)) st->ackMs = now;
}

static void applyStationLatch(const char *id, const char *name) {
  if (!showduino_emergency_id_ok(id)) {
    /* No recordable identity (for example a full table). Still latch. */
    if (!emergencyLocked) triggerEmergencyWireless();
    return;
  }
  EmergencyNodeStatus *st = allocSlot(id, nullptr);
  if (st) {
    /* MISSION 001I: a bare ASSERT never sets online or lastRxMs. Only an accepted,
     * identity-validated ANNOUNCE (or STATUS from a single online station) does. */
    st->seen = true;
    st->latched = true;
    strncpy(st->id, id, sizeof(st->id) - 1);
    if (name && name[0]) strncpy(st->name, name, sizeof(st->name) - 1);
    strncpy(st->state, "LATCHED", sizeof(st->state) - 1);
  }
  const int first = showduino_emergency_authority_assert(&sAuth, id, name);
  if (first && !emergencyLocked) {
    SD_LOGI("ESTOP", "EMERGENCY ASSERT SOURCE=%s NAME=%s",
            id, (name && name[0]) ? name : "-");
    triggerEmergencyWireless(); /* RAM and outputs first, then storage */
  } else {
    if (showduino_log_rate_ok(&sDupLogMs, millis(), 2000UL)) {
      SD_LOGI("ESTOP", "EMERGENCY DUPLICATE SOURCE=%s", id);
    }
  }
  /* MISSION 001J: the episode id is stored after actuation, then an authenticated
   * ACK teaches the station the episode. No unauthenticated ACK is sent. */
  ensureEpisode();
  ackIfDue(st);
}

void emergencyNodeLinkBegin() {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    sNodes[i] = EmergencyNodeStatus();
  }
  /* Restored clear blockers come back as pressed, unseen, offline records.
   * They keep blocking until a fresh released report from the same identity. */
  for (uint8_t k = 0; k < sStagedCount; ++k) {
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (sNodes[i].used) continue;
      sNodes[i].used = true;
      sNodes[i].inputOpen = true;
      sNodes[i].asserted = true;
      strncpy(sNodes[i].id, sStaged[k].id, sizeof(sNodes[i].id) - 1);
      strncpy(sNodes[i].mac, sStaged[k].mac, sizeof(sNodes[i].mac) - 1);
      strncpy(sNodes[i].state, "OFFLINE", sizeof(sNodes[i].state) - 1);
      break;
    }
  }
  sBooted = true;
  /* MISSION 001J: keys and the counter-block limit. A failure blocks clear. */
  stationKeysBegin();
  if (stationKeysFault()) {
    SD_LOGE("ESTOP", "station key record unreadable — clear blocked; EMERGENCY:KEY:RESET on USB");
  }
  uint32_t limit = 0;
  sCounterLoaded = stationCounterLoad(&limit);
  if (sCounterLoaded) {
    showduino_auth_counter_boot(&sCtr, limit);
  } else {
    sCounterFault = true;
    SD_LOGE("ESTOP", "station counter unreadable — station commands held, clear blocked");
  }
  showduino_auth_chal_reset(&sChal);
  sEpisode = 0;
  sTableOverflow = false;
  sGclReady = false;
  showduino_emergency_authority_init(&sAuth);
  sLastWire[0] = 0;
  sLastDetail[0] = 0;
  sLastSafety[0] = 0;
  sLastSource[0] = 0;
}

void emergencyNodeLinkLoop() {
  /* Keep each station's own latch consistent with the P4. Throttled to once per
   * second per station, and each case stops when that station's announce agrees. */
  /* MISSION 001J: no unauthenticated ACK or GLOBAL:OBSERVED is ever sent. The
   * episode is made durable, stations are challenged, and a station whose verified
   * report shows it latched without this episode gets an authenticated ACK. */
  ensureEpisode();
  const uint32_t nowMs = millis();
  const uint32_t period = emergencyLocked ? SHOWDUINO_STATION_CHAL_LATCHED_MS
                                          : SHOWDUINO_STATION_CHAL_IDLE_MS;
  if (stationKeysCount() > 0 && (!emergencyLocked || sEpisode) &&
      (!sChalMs || (nowMs - sChalMs) >= period)) {
    (void)issueChallenge();
  }
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.online || !st.id[0]) continue;
    if (st.announcedLatched && !st.ackedReport) ackIfDue(&st);
  }
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.online || !st.lastRxMs) continue;
    if ((millis() - st.lastRxMs) > SHOWDUINO_EMERGENCY_COMMS_TIMEOUT_MS) {
      st.online = false;
      strncpy(st.state, "OFFLINE", sizeof(st.state) - 1);
      SD_LOGW("ESTOP", "EMERGENCY NODE OFFLINE %s %s",
              st.id[0] ? st.id : "-",
              st.name[0] ? st.name : "-");
      publishState(true);
    }
  }
  if ((millis() - sKeepaliveMs) >= SHOWDUINO_EMERGENCY_HEARTBEAT_MS) {
    sKeepaliveMs = millis();
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online) continue;
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_OWN_GRANT);
      routeTo(sNodes[i].id, sSeq++, SHOWDUINO_ESTOP_STATUS);
      if (sNodes[i].pixelCapable) {
        routeTo(sNodes[i].id, sSeq++, "PIXEL:OWN:GRANT");
        routeTo(sNodes[i].id, sSeq++, "PIXEL:STATUS");
      }
    }
  }
  if ((millis() - sPublishMs) >= 3000UL) publishState(true);
}

bool emergencyNodeLinkHandleReport(const char *line) {
  if (!line || strncmp(line, "NODE:EMERGENCY:", 15) != 0) return false;
  const char *p = line + 15;

  if (showduino_emergency_is_clear_token(p) &&
      strcmp(p, "EMERGENCY:CLEAR") != 0) {
    SD_LOGW("ESTOP", "Rejected node clear token");
    (void)showduino_emergency_authority_apply_node_clear(&sAuth, p);
    return true;
  }

  /* MISSION 001J: one interpretation for AUTH, ASSERT, ANNOUNCE and STATUS. The
   * shared model (protocol/showduino_station_model.h) decides; only a verified AUTH
   * changes station state. This code performs the effects only. */
  if (!strncmp(p, "AUTH:", 5) || !strncmp(p, "ANNOUNCE:", 9) ||
      showduino_emergency_is_assert(p) || !strncmp(p, "STATUS:", 7)) {
    ShowduinoStationModel table[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
    bool prevOnline[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
    bool prevOpen[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
    loadStationModel(table, prevOnline, prevOpen);
    ShowduinoStationAuthPolicy pol{showduino_auth_hmac_sha256_mbedtls, &sChal, sEpisode,
                                   millis(), keyForCb, nullptr};
    ShowduinoStationResult res{};
    const int handled = showduino_station_interpret_line(table, SHOWDUINO_EMERGENCY_NODE_MAX_NODES,
                                                         p, &pol, &res);
    if (!handled) return true;
    storeStationModel(table);
    if (res.tableFull && !sTableOverflow) {
      sTableOverflow = true;
      SD_LOGW("ESTOP", "station table full — clear blocked until EMERGENCY:STATION:DISMISS on USB");
    }

    if (res.kind == SHOWDUINO_STATION_LINE_ASSERT) {
      if (res.globalLatch) applyStationLatch(res.ackId, res.name);
      emergencyNodeLinkPersistBlockers(); /* after actuation (MISSION 001F) */
      publishState(true);
      return true;
    }

    if (res.kind == SHOWDUINO_STATION_LINE_AUTH) {
      if (!res.verified) {
        if (showduino_log_rate_ok(&sAuthRejectLogMs, millis(), 5000UL)) {
          SD_LOGW("ESTOP", "station AUTH rejected (key, nonce, episode or freshness)");
        }
        if (res.globalLatch) applyStationLatch(res.ackId, res.name);
        return true;
      }
      EmergencyNodeStatus *st = &sNodes[res.slot];
      st->seen = true;
      st->lastRxMs = millis();
      st->latched = st->announcedLatched;
      st->acked = st->ackedReport;
      strncpy(st->state, st->inputOpen ? "PRESSED" : (st->latched ? "LATCHED" : "NORMAL"),
              sizeof(st->state) - 1);
      if (!prevOnline[res.slot]) {
        SD_LOGI("ESTOP", "EMERGENCY NODE VERIFIED %s %s", st->id[0] ? st->id : "-",
                st->name[0] ? st->name : "-");
      }
      (void)prevOpen;
      if (res.globalLatch) applyStationLatch(res.ackId, st->name);
      if (emergencyLocked && st->announcedLatched && !st->ackedReport) ackIfDue(st);
      emergencyNodeLinkPersistBlockers();
      publishState();
      return true;
    }

    if (res.kind == SHOWDUINO_STATION_LINE_ANNOUNCE) {
      /* Display only: name and firmware labels for a station already in the table. */
      ShowduinoEmergencyAnnounce an{};
      if (showduino_emergency_parse_announce(p, &an)) {
        EmergencyNodeStatus *st = findById(an.id);
        if (st) {
          if (an.name[0]) strncpy(st->name, an.name, sizeof(st->name) - 1);
          if (an.firmware[0]) strncpy(st->firmware, an.firmware, sizeof(st->firmware) - 1);
        }
      }
      return true;
    }
    return true; /* STATUS: display only */
  }

  if (!strncmp(p, "ESTOP:CAPS:", 11) || !strncmp(p, "EMERGENCY:CAPS:", 15)) {
    const char *caps = strchr(p, ':');
    if (caps) caps = strchr(caps + 1, ':');
    if (caps) {
      ++caps;
      EmergencyNodeStatus *only = nullptr;
      uint8_t nOn = 0;
      for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
        if (sNodes[i].used && sNodes[i].online) {
          nOn++;
          only = &sNodes[i];
        }
      }
      if (nOn == 1 && only && strstr(caps, "PIXEL")) {
        only->pixelCapable = true;
      }
    }
    return true;
  }
  if (!strncmp(p, "PIXEL:CAPS:", 11)) {
    EmergencyNodeStatus *only = nullptr;
    uint8_t nOn = 0;
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (sNodes[i].used && sNodes[i].online) {
        nOn++;
        only = &sNodes[i];
      }
    }
    if (nOn == 1 && only) {
      strncpy(only->pixelCaps, p + 11, sizeof(only->pixelCaps) - 1);
      only->pixelCapable = true;
    }
    return true;
  }
  if (!strncmp(p, "PIXEL:STATUS:", 13)) {
    EmergencyNodeStatus *only = nullptr;
    uint8_t nOn = 0;
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (sNodes[i].used && sNodes[i].online) {
        nOn++;
        only = &sNodes[i];
      }
    }
    if (nOn == 1 && only) {
      only->pixelCapable = true;
      const char *body = p + 13;
      only->pixelReady = (strncmp(body, "READY", 5) == 0);
      const char *gpio = strstr(body, "GPIO=");
      const char *cfg = strstr(body, "CONFIGURED=");
      const char *cnt = strstr(body, "COUNT=");
      const char *mx = strstr(body, "MAX=");
      const char *bri = strstr(body, "BRIGHTNESS=");
      const char *em = strstr(body, "EMERGENCY=");
      if (gpio) only->pixelPin = (uint8_t)atoi(gpio + 5);
      if (cfg) only->pixelConfigured = (uint16_t)atoi(cfg + 11);
      if (cnt) only->pixelCount = (uint16_t)atoi(cnt + 6);
      if (mx) only->pixelMax = (uint16_t)atoi(mx + 4);
      if (bri) only->pixelBrightness = (uint8_t)atoi(bri + 11);
      if (em) only->pixelEmergency = atoi(em + 10) != 0;
    }
    return true;
  }

  return true;
}

bool emergencyNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command) return false;
  if (showduino_emergency_is_clear_token(command) &&
      strncmp(command, "EMERGENCY:CLEAR", 15) != 0) {
    if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NO_CLEAR", replyLen - 1);
    return true;
  }

  /* Same-peer Pixel capability — strip ESTOP:NODE:PIXEL: → PIXEL: */
  if (!strncmp(command, "ESTOP:NODE:PIXEL:", 17) ||
      !strncmp(command, "EMERGENCY:NODE:PIXEL:", 21)) {
    const char *pixelCmd = command + (!strncmp(command, "ESTOP:NODE:PIXEL:", 17) ? 17 : 21);
    char forwarded[120];
    if (!pixelCmd[0]) {
      if (reply && replyLen) strncpy(reply, "ERR:ESTOP:NODE:PIXEL:BAD_COMMAND", replyLen - 1);
      return true;
    }
    if (!strncmp(pixelCmd, "PIXEL:", 6)) {
      strncpy(forwarded, pixelCmd, sizeof(forwarded) - 1);
    } else {
      snprintf(forwarded, sizeof(forwarded), "PIXEL:%s", pixelCmd);
    }
    forwarded[sizeof(forwarded) - 1] = '\0';

    const bool alwaysOk =
        !strcmp(forwarded, "PIXEL:STATUS") ||
        !strcmp(forwarded, "PIXEL:OFF") ||
        !strcmp(forwarded, "PIXEL:BLACKOUT");
    if (emergencyLocked && !alwaysOk) {
      if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NODE:PIXEL:EMERGENCY_ACTIVE", replyLen - 1);
      return true;
    }

    EmergencyNodeStatus *target = nullptr;
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online) continue;
      if (!target) target = &sNodes[i];
      if (sNodes[i].pixelCapable) {
        target = &sNodes[i];
        break;
      }
    }
    if (!target || !target->id[0]) {
      if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NODE:PIXEL:OFFLINE", replyLen - 1);
      return true;
    }

    const uint32_t seq = sSeq++;
    routeTo(target->id, seq, forwarded);
    if (reply && replyLen) {
      snprintf(reply, replyLen, "ESTOP:NODE:PIXEL:PENDING:%lu", (unsigned long)seq);
    }
    return true;
  }

  /* ESTOP:NODE:ESTOP-01:PIXEL:... */
  if (!strncmp(command, "ESTOP:NODE:", 11) || !strncmp(command, "EMERGENCY:NODE:", 15)) {
    const char *p = !strncmp(command, "ESTOP:NODE:", 11) ? command + 11 : command + 15;
    const char *c = strchr(p, ':');
    if (c && !strncmp(c + 1, "PIXEL:", 6)) {
      char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
      size_t n = (size_t)(c - p);
      if (n > SHOWDUINO_EMERGENCY_ID_MAX) n = SHOWDUINO_EMERGENCY_ID_MAX;
      memcpy(id, p, n);
      id[n] = 0;
      const char *pixelCmd = c + 1;
      char forwarded[120];
      strncpy(forwarded, pixelCmd, sizeof(forwarded) - 1);
      forwarded[sizeof(forwarded) - 1] = '\0';
      const bool alwaysOk =
          !strcmp(forwarded, "PIXEL:STATUS") ||
          !strcmp(forwarded, "PIXEL:OFF") ||
          !strcmp(forwarded, "PIXEL:BLACKOUT");
      if (emergencyLocked && !alwaysOk) {
        if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NODE:PIXEL:EMERGENCY_ACTIVE", replyLen - 1);
        return true;
      }
      EmergencyNodeStatus *st = findById(id);
      if (!st || !st->online) {
        if (reply && replyLen) strncpy(reply, "REJECTED:ESTOP:NODE:PIXEL:OFFLINE", replyLen - 1);
        return true;
      }
      const uint32_t seq = sSeq++;
      routeTo(st->id, seq, forwarded);
      if (reply && replyLen) {
        snprintf(reply, replyLen, "ESTOP:NODE:PIXEL:PENDING:%lu", (unsigned long)seq);
      }
      return true;
    }
  }

  if (!strcmp(command, "ESTOP:STATUS") || !strncmp(command, "ESTOP:NODE:", 11) ||
      !strncmp(command, "EMERGENCY:NODE:", 15)) {
    emergencyNodeLinkPublishToDirector();
    if (reply && replyLen) strncpy(reply, "OK:ESTOP:STATUS", replyLen - 1);
    return true;
  }
  return false;
}

void emergencyNodeLinkOnEmergency(bool active) {
  if (!active) {
    showduino_emergency_authority_legitimate_clear(&sAuth);
    /* MISSION 001J: stations return to NORMAL only on an authenticated GCL for the
     * episode they latched under. Counters were reserved by the confirm. */
    if (sGclReady && sGclEpisode) {
      for (uint8_t i = 0; i < sGclCount; ++i) {
        (void)sendCmd(sGclIds[i], "GCL", sGclEpisode, &sGclCounters[i]);
      }
    }
    sGclReady = false;
    sEpisode = 0;
    sClearRequestSeq = 0;
    showduino_auth_chal_reset(&sChal);
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].id[0]) continue;
      routeTo(sNodes[i].id, sSeq++, "STATE:EMERGENCY:CLEAR"); /* indication only */
      sNodes[i].acked = false;
      sNodes[i].ackMs = 0;
    }
    sLastSource[0] = 0;
  } else {
    ensureEpisode();
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      if (!sNodes[i].used || !sNodes[i].online || !sNodes[i].id[0]) continue;
      routeTo(sNodes[i].id, sSeq++, "STATE:EMERGENCY:ACTIVE"); /* indication only */
    }
  }
  publishState(true);
}

/* ---------- MISSION 001J: local USB maintenance ---------- */

bool emergencyNodeLinkIsMaintenanceCommand(const char *c) {
  return c && (!strncmp(c, "EMERGENCY:KEY:", 14) || !strncmp(c, "EMERGENCY:STATION:", 18) ||
               !strncmp(c, "EMERGENCY:AUTH:", 15));
}

static bool takeId(const char *src, char *out, size_t n, const char **rest) {
  const char *c = strchr(src, ':');
  const size_t len = c ? (size_t)(c - src) : strlen(src);
  if (len == 0 || len >= n) return false;
  memcpy(out, src, len);
  out[len] = 0;
  if (rest) *rest = c ? c + 1 : nullptr;
  return showduino_emergency_id_ok(out) != 0;
}

bool emergencyNodeLinkHandleMaintenance(const char *command, bool localUsb) {
  if (!emergencyNodeLinkIsMaintenanceCommand(command)) return false;
  if (!localUsb) {
    /* Never echo the command: it may carry key material. */
    Serial.println("[ESTOP] REJECTED station maintenance command — local USB console only");
    SD_LOGW("ESTOP", "station maintenance command refused from Comms/Web");
    return true;
  }
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  const char *rest = nullptr;

  if (!strncmp(command, "EMERGENCY:KEY:SET:", 18)) {
    if (!takeId(command + 18, id, sizeof(id), &rest) || !rest ||
        strlen(rest) != SHOWDUINO_AUTH_KEY_HEX) {
      Serial.println("[ESTOP] KEY:SET usage: EMERGENCY:KEY:SET:ESTOP-nn:<64 hex chars>");
      return true;
    }
    uint8_t key[SHOWDUINO_AUTH_KEY_BYTES];
    const bool parsed = showduino_auth_hex_decode(rest, sizeof(key), key) != 0;
    const bool ok = parsed && stationKeysSet(id, key);
    memset(key, 0, sizeof(key));
    char fp[9];
    stationKeysFingerprint(id, fp);
    Serial.printf("[ESTOP] KEY:SET %s %s fp=%s\n", id, ok ? "OK" : "FAILED", ok ? fp : "-");
    SD_LOGW("ESTOP", "MAINTENANCE station key %s for %s (fp=%s)", ok ? "set" : "set FAILED", id,
            ok ? fp : "-");
    if (ok && emergencyLocked) {
      Serial.println("[ESTOP] latched: the station still blocks clear until it proves RELEASED with this key");
    }
    return true;
  }
  if (!strncmp(command, "EMERGENCY:KEY:REVOKE:", 21)) {
    if (!takeId(command + 21, id, sizeof(id), nullptr)) {
      Serial.println("[ESTOP] KEY:REVOKE usage: EMERGENCY:KEY:REVOKE:ESTOP-nn");
      return true;
    }
    const bool ok = stationKeysRevoke(id);
    Serial.printf("[ESTOP] KEY:REVOKE %s %s\n", id, ok ? "OK" : "FAILED");
    SD_LOGW("ESTOP", "MAINTENANCE station key revoke %s %s", id, ok ? "OK" : "FAILED");
    return true;
  }
  if (!strcmp(command, "EMERGENCY:KEY:LIST")) {
    Serial.printf("[ESTOP] KEYS count=%u fault=%u\n", (unsigned)stationKeysCount(),
                  stationKeysFault() ? 1U : 0U);
    for (uint8_t i = 0; i < stationKeysCount(); ++i) {
      char fp[9];
      stationKeysFingerprint(stationKeysId(i), fp);
      Serial.printf("[ESTOP]   %s fp=%s\n", stationKeysId(i), fp);
    }
    return true;
  }
  if (!strcmp(command, "EMERGENCY:KEY:RESET")) {
    if (!stationKeysFault()) {
      Serial.println("[ESTOP] KEY:RESET refused — record is readable (use REVOKE per station)");
      return true;
    }
    const bool ok = stationKeysReset();
    Serial.printf("[ESTOP] KEY:RESET %s — re-enter every station key\n", ok ? "OK" : "FAILED");
    SD_LOGW("ESTOP", "MAINTENANCE station key record reset %s", ok ? "OK" : "FAILED");
    return true;
  }
  if (!strncmp(command, "EMERGENCY:STATION:DISMISS:", 26)) {
    if (!takeId(command + 26, id, sizeof(id), nullptr)) {
      Serial.println("[ESTOP] DISMISS usage: EMERGENCY:STATION:DISMISS:ESTOP-nn");
      return true;
    }
    ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES];
    loadStationModel(t, nullptr, nullptr);
    const int ok = showduino_station_model_dismiss(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, id,
                                                   isProvisioned(id) ? 1 : 0);
    if (ok) {
      storeStationModel(t);
      for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
        if (!sNodes[i].used) sNodes[i] = EmergencyNodeStatus();
      }
      sTableOverflow = false;
      emergencyNodeLinkPersistBlockers();
      publishState(true);
    }
    Serial.printf("[ESTOP] DISMISS %s %s\n", id,
                  ok ? "OK — check that physical station by hand"
                     : "REFUSED (provisioned: it must prove RELEASED or be revoked first; or unknown)");
    SD_LOGW("ESTOP", "MAINTENANCE station dismiss %s %s", id, ok ? "OK" : "REFUSED");
    return true;
  }
  if (!strcmp(command, "EMERGENCY:AUTH:STATUS")) {
    Serial.printf("[ESTOP] AUTH keys=%u keyFault=%u counterFault=%u loaded=%u episode=%lu "
                  "nonce=%s chalSeq=%lu overflow=%u\n",
                  (unsigned)stationKeysCount(), stationKeysFault() ? 1U : 0U,
                  sCounterFault ? 1U : 0U, sCounterLoaded ? 1U : 0U, (unsigned long)sEpisode,
                  sNonce.seeded ? "seeded" : "NOT SEEDED", (unsigned long)sChal.lastSeq,
                  sTableOverflow ? 1U : 0U);
    for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
      const EmergencyNodeStatus &st = sNodes[i];
      if (!st.used) continue;
      Serial.printf("[ESTOP]   %s prov=%u verified=%u online=%u pressed=%u latched=%u acked=%u "
                    "asserted=%u proofSeq=%lu\n",
                    st.id[0] ? st.id : "-", isProvisioned(st.id) ? 1U : 0U,
                    st.reportKnown ? 1U : 0U, st.online ? 1U : 0U, st.inputOpen ? 1U : 0U,
                    st.announcedLatched ? 1U : 0U, st.ackedReport ? 1U : 0U,
                    st.asserted ? 1U : 0U, (unsigned long)st.proofChalSeq);
    }
    return true;
  }
  Serial.println("[ESTOP] station maintenance: KEY:SET|KEY:REVOKE|KEY:LIST|KEY:RESET|"
                 "STATION:DISMISS|AUTH:STATUS");
  return true;
}

void emergencyNodeLinkAppendJsonArray(String &json) {
  json += "[\n";
  bool first = true;
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    const EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    const uint32_t age = (st.lastRxMs && st.online) ? (millis() - st.lastRxMs) : 0;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"type\": \"EMERGENCY\",\n";
    json += "      \"seen\": true,\n      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"input\": \"";
    json += st.inputOpen ? "OPEN" : "CLOSED";
    json += "\",\n      \"latch\": \"";
    json += st.latched ? "LATCHED" : "CLEAR";
    json += "\",\n      \"acked\": ";
    json += st.acked ? "true" : "false";
    json += ",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmware\": \"";
    json += st.firmware;
    json += "\",\n      \"pixelCapable\": ";
    json += st.pixelCapable ? "true" : "false";
    json += ",\n      \"lastContactMs\": ";
    json += String((unsigned long)age);
    json += "\n    }";
  }
  json += "\n  ]";
}

void emergencyNodeLinkAppendDevicesJson(String &json, bool &first) {
  for (uint8_t i = 0; i < SHOWDUINO_EMERGENCY_NODE_MAX_NODES; ++i) {
    const EmergencyNodeStatus &st = sNodes[i];
    if (!st.used || !st.seen) continue;
    if (!first) json += ",\n";
    first = false;
    json += "    {\n";
    json += "      \"id\": \"";
    json += st.id;
    json += "\",\n      \"name\": \"Emergency Node\",\n";
    json += "      \"friendlyName\": \"";
    json += st.name[0] ? st.name : st.id;
    json += "\",\n      \"board\": \"ESP32-C3\",\n";
    json += "      \"role\": \"EMERGENCY\",\n";
    json += "      \"online\": ";
    json += st.online ? "true" : "false";
    json += ",\n      \"connectionStatus\": \"espnow-node\",\n";
    json += "      \"mac\": \"";
    json += st.mac;
    json += "\",\n      \"firmwareVersion\": \"";
    json += st.firmware;
    json += "\",\n      \"state\": \"";
    json += st.state;
    json += "\",\n      \"capabilities\": [\"EMERGENCY\"";
    if (st.pixelCapable) json += ", \"PIXEL\"";
    json += "],\n      \"pixelCapable\": ";
    json += st.pixelCapable ? "true" : "false";
    json += "\n    }";
  }
}
