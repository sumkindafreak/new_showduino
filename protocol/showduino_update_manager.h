#ifndef SHOWDUINO_UPDATE_MANAGER_H
#define SHOWDUINO_UPDATE_MANAGER_H

/*
 * Showduino system-update manager — Phase 1 foundation.
 *
 * Release manifest + live inventory + sequential Emergency gate.
 * Phase 2A: Comms self-OTA eligibility only. Generic APPLY stays blocked.
 * Other components remain otaCapable=false.
 *
 * Emergency Nodes reuse showduino_emergency_update_gate().
 * Do not invent a second, weaker safety gate.
 *
 *   ESTOP-01 update -> reboot -> HEALTHY + LINKED -> ESTOP-02 ...
 *
 * Protocol 1.0 additive. Product / SHDO unchanged.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "showduino_version.h"
#include "showduino_emergency_node.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHOWDUINO_UPDATE_SCHEMA_NAME        "showduino-release-v1"
#define SHOWDUINO_UPDATE_SCHEMA_VERSION     1

#define SHOWDUINO_UPDATE_ROLE_P4            "p4"
#define SHOWDUINO_UPDATE_ROLE_COMMS         "comms"
#define SHOWDUINO_UPDATE_ROLE_DIRECTOR      "director"
#define SHOWDUINO_UPDATE_ROLE_AUDIO         "audio"
#define SHOWDUINO_UPDATE_ROLE_LAMP          "lamp"
#define SHOWDUINO_UPDATE_ROLE_PIXEL         "pixel"
#define SHOWDUINO_UPDATE_ROLE_EMERGENCY     "emergency"

#define SHOWDUINO_UPDATE_POLICY_INDEPENDENT "INDEPENDENT"
#define SHOWDUINO_UPDATE_POLICY_ONE_AT_TIME SHOWDUINO_EMERGENCY_UPDATE_POLICY

#define SHOWDUINO_UPDATE_MAX_COMPONENTS     12
#define SHOWDUINO_UPDATE_MAX_INVENTORY      24
#define SHOWDUINO_UPDATE_MAX_PLAN_STEPS     16
#define SHOWDUINO_UPDATE_ID_MAX             16
#define SHOWDUINO_UPDATE_FW_MAX             20
#define SHOWDUINO_UPDATE_ROLE_MAX           16

#define SHOWDUINO_UPDATE_FAULT_SAFETY       "SAFETY_NODE_UPDATE_FAILED"
#define SHOWDUINO_UPDATE_BLOCK_OTA          "OTA_NOT_IMPLEMENTED"
#define SHOWDUINO_UPDATE_BLOCK_SHOW         "SHOW_RUNNING"
#define SHOWDUINO_UPDATE_BLOCK_EMERGENCY    "EMERGENCY_ACTIVE"
#define SHOWDUINO_UPDATE_BLOCK_GATE         "SAFETY_GATE_HOLD"
#define SHOWDUINO_UPDATE_BLOCK_COMPONENT    "OTA_UNAVAILABLE"
#define SHOWDUINO_UPDATE_BLOCK_HARDWARE     "WRONG_HARDWARE"
#define SHOWDUINO_UPDATE_BLOCK_ROLE         "WRONG_ROLE"
#define SHOWDUINO_UPDATE_BLOCK_SAME         "SAME_VERSION"
#define SHOWDUINO_UPDATE_BLOCK_DOWNGRADE    "DOWNGRADE"
#define SHOWDUINO_UPDATE_BLOCK_MANIFEST     "BAD_MANIFEST"
#define SHOWDUINO_UPDATE_BLOCK_SHA          "BAD_SHA256"
#define SHOWDUINO_UPDATE_BLOCK_SIZE         "OVERSIZE"
#define SHOWDUINO_UPDATE_BLOCK_MAINT        "MAINTENANCE_REQUIRED"
#define SHOWDUINO_UPDATE_BLOCK_P4           "P4_OFFLINE"
#define SHOWDUINO_UPDATE_BLOCK_CONFIRM      "CONFIRM_REQUIRED"
#define SHOWDUINO_UPDATE_BLOCK_INTERNET     "INTERNET_UNAVAILABLE"
#define SHOWDUINO_UPDATE_BLOCK_PROTOCOL     "PROTOCOL_INCOMPATIBLE"
#define SHOWDUINO_UPDATE_FAULT_INTEGRITY    "FIRMWARE_INTEGRITY_FAILED"
#define SHOWDUINO_UPDATE_FAULT_EMERGENCY    "INTERRUPTED_BY_EMERGENCY"
#define SHOWDUINO_UPDATE_CHECK_NO_INTERNET  "Internet connection unavailable"
#define SHOWDUINO_COMMS_BIN_FILENAME        "ShowduinoS3CommsController.ino.bin"

#define SHOWDUINO_COMMS_HARDWARE_ID         "SHOWDUINO-S3-COMMS-V1"
#define SHOWDUINO_COMMS_OTA_SLOT_BYTES      3342336u
#define SHOWDUINO_OTA_SHA256_HEX_LEN        64
#define SHOWDUINO_COMMS_OTA_VALIDATE_MS     20000u

#define SHOWDUINO_OTA_STATE_IDLE            "IDLE"
#define SHOWDUINO_OTA_STATE_DOWNLOADING     "DOWNLOADING"
#define SHOWDUINO_OTA_STATE_VERIFYING       "VERIFYING"
#define SHOWDUINO_OTA_STATE_INSTALLING      "INSTALLING"
#define SHOWDUINO_OTA_STATE_REBOOT          "REBOOT_REQUIRED"
#define SHOWDUINO_OTA_STATE_PENDING         "PENDING_VALIDATION"
#define SHOWDUINO_OTA_STATE_COMPLETE        "COMPLETE"
#define SHOWDUINO_OTA_STATE_ROLLED_BACK     "ROLLED_BACK"
#define SHOWDUINO_OTA_STATE_FAILED          "FAILED"
#define SHOWDUINO_OTA_STATE_EMERGENCY       "INTERRUPTED_BY_EMERGENCY"

#define SHOWDUINO_OTA_EVT_START             1
#define SHOWDUINO_OTA_EVT_DOWNLOAD_OK       2
#define SHOWDUINO_OTA_EVT_HASH_OK           3
#define SHOWDUINO_OTA_EVT_WRITE_OK          4
#define SHOWDUINO_OTA_EVT_REBOOTED          5
#define SHOWDUINO_OTA_EVT_HEALTH_OK         6
#define SHOWDUINO_OTA_EVT_DOWNLOAD_FAIL     7
#define SHOWDUINO_OTA_EVT_HASH_FAIL         8
#define SHOWDUINO_OTA_EVT_WRITE_FAIL        9
#define SHOWDUINO_OTA_EVT_EMERGENCY        10
#define SHOWDUINO_OTA_EVT_HEALTH_FAIL      11
#define SHOWDUINO_OTA_EVT_ROLLBACK_DONE    12

#define SHOWDUINO_UPDATE_STEP_READY         "READY"
#define SHOWDUINO_UPDATE_STEP_HOLD          "HOLD"
#define SHOWDUINO_UPDATE_STEP_FAILED        "FAILED"
#define SHOWDUINO_UPDATE_STEP_SKIPPED       "SKIPPED"

typedef struct ShowduinoReleaseComponent {
  char role[SHOWDUINO_UPDATE_ROLE_MAX + 1];
  char id[SHOWDUINO_UPDATE_ID_MAX + 1];
  char firmware[SHOWDUINO_UPDATE_FW_MAX + 1];
  uint8_t ota_capable;      /* Phase 2A: comms may be 1 */
  uint8_t one_at_a_time;    /* Emergency stations */
  char hardware_id[32];
  char filename[48];
  char sha256[SHOWDUINO_OTA_SHA256_HEX_LEN + 1];
  uint32_t size;
} ShowduinoReleaseComponent;

typedef struct ShowduinoReleaseManifest {
  char schema[24];
  int schema_version;
  char product[16];
  char product_version[24];
  char protocol[8];
  int shdo;
  uint8_t ota_install;      /* Phase 1: always 0 */
  uint8_t component_count;
  ShowduinoReleaseComponent components[SHOWDUINO_UPDATE_MAX_COMPONENTS];
} ShowduinoReleaseManifest;

typedef struct ShowduinoUpdateInventoryItem {
  char role[SHOWDUINO_UPDATE_ROLE_MAX + 1];
  char id[SHOWDUINO_UPDATE_ID_MAX + 1];
  char name[24];
  char firmware[SHOWDUINO_UPDATE_FW_MAX + 1];
  uint8_t present;
  uint8_t online;
  uint8_t healthy;          /* booted, input readable, no local fault */
  uint8_t linked;           /* fresh announce / heartbeat */
  uint8_t ota_capable;
  uint8_t safety_class;     /* 1 = Emergency Node */
} ShowduinoUpdateInventoryItem;

typedef struct ShowduinoUpdateInventory {
  uint8_t count;
  ShowduinoUpdateInventoryItem items[SHOWDUINO_UPDATE_MAX_INVENTORY];
} ShowduinoUpdateInventory;

typedef struct ShowduinoUpdatePlanStep {
  char role[SHOWDUINO_UPDATE_ROLE_MAX + 1];
  char id[SHOWDUINO_UPDATE_ID_MAX + 1];
  char state[12];
  char hold_reason[72];
  char fault[40];
  uint8_t one_at_a_time;
  uint8_t allowed;
} ShowduinoUpdatePlanStep;

typedef struct ShowduinoUpdatePlan {
  uint8_t apply_implemented; /* Phase 1: always 0 */
  uint8_t blocked;
  char blocked_reason[40];
  uint8_t safety_failed;
  uint8_t count;
  ShowduinoUpdatePlanStep steps[SHOWDUINO_UPDATE_MAX_PLAN_STEPS];
} ShowduinoUpdatePlan;

static inline void showduino_update_copy(char *dst, size_t cap, const char *src) {
  if (!dst || cap == 0) return;
  dst[0] = 0;
  if (!src) return;
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}

static inline int showduino_update_role_ok(const char *role) {
  if (!role || !role[0]) return 0;
  return strcmp(role, SHOWDUINO_UPDATE_ROLE_P4) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_COMMS) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_DIRECTOR) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_AUDIO) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_LAMP) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_PIXEL) == 0 ||
         strcmp(role, SHOWDUINO_UPDATE_ROLE_EMERGENCY) == 0;
}

static inline int showduino_update_is_emergency_role(const char *role) {
  return role && strcmp(role, SHOWDUINO_UPDATE_ROLE_EMERGENCY) == 0;
}

static inline int showduino_update_is_comms_role(const char *role) {
  return role && strcmp(role, SHOWDUINO_UPDATE_ROLE_COMMS) == 0;
}

static inline const char *showduino_update_policy_for_role(const char *role) {
  return showduino_update_is_emergency_role(role)
             ? SHOWDUINO_UPDATE_POLICY_ONE_AT_TIME
             : SHOWDUINO_UPDATE_POLICY_INDEPENDENT;
}

static inline void showduino_release_manifest_clear(ShowduinoReleaseManifest *m) {
  if (!m) return;
  memset(m, 0, sizeof(*m));
  showduino_update_copy(m->schema, sizeof(m->schema), SHOWDUINO_UPDATE_SCHEMA_NAME);
  m->schema_version = SHOWDUINO_UPDATE_SCHEMA_VERSION;
  showduino_update_copy(m->product, sizeof(m->product), SHOWDUINO_PRODUCT_NAME);
  showduino_update_copy(m->product_version, sizeof(m->product_version),
                        SHOWDUINO_PLATFORM_VERSION);
  snprintf(m->protocol, sizeof(m->protocol), "%d.%d",
           SHOWDUINO_PROTOCOL_VERSION_MAJOR, SHOWDUINO_PROTOCOL_VERSION_MINOR);
  m->shdo = SHOWDUINO_SHDO_PACKAGE_VERSION;
  m->ota_install = 0;
}

static inline int showduino_release_manifest_add(ShowduinoReleaseManifest *m,
                                                 const char *role,
                                                 const char *id,
                                                 const char *firmware,
                                                 int one_at_a_time) {
  ShowduinoReleaseComponent *c;
  if (!m || !showduino_update_role_ok(role)) return 0;
  if (m->component_count >= SHOWDUINO_UPDATE_MAX_COMPONENTS) return 0;
  c = &m->components[m->component_count++];
  memset(c, 0, sizeof(*c));
  showduino_update_copy(c->role, sizeof(c->role), role);
  showduino_update_copy(c->id, sizeof(c->id), id);
  showduino_update_copy(c->firmware, sizeof(c->firmware), firmware);
  c->ota_capable = 0;
  c->one_at_a_time = one_at_a_time ? 1 : 0;
  return 1;
}

static inline int showduino_release_manifest_valid(const ShowduinoReleaseManifest *m) {
  uint8_t i;
  if (!m) return 0;
  if (strcmp(m->schema, SHOWDUINO_UPDATE_SCHEMA_NAME) != 0) return 0;
  if (m->schema_version != SHOWDUINO_UPDATE_SCHEMA_VERSION) return 0;
  if (m->shdo != SHOWDUINO_SHDO_PACKAGE_VERSION) return 0;
  {
    int proto_major = 0;
    const char *ps = m->protocol;
    if (!ps || *ps < '0' || *ps > '9') return 0;
    while (*ps >= '0' && *ps <= '9') {
      proto_major = proto_major * 10 + (*ps - '0');
      ++ps;
    }
    if (!showduino_protocol_compatible(proto_major)) return 0;
  }
  if (m->ota_install) return 0; /* system-wide OTA remains false */
  for (i = 0; i < m->component_count; i++) {
    if (!showduino_update_role_ok(m->components[i].role)) return 0;
    if (m->components[i].ota_capable &&
        !showduino_update_is_comms_role(m->components[i].role)) {
      return 0;
    }
    if (showduino_update_is_emergency_role(m->components[i].role) &&
        m->components[i].ota_capable) {
      return 0;
    }
  }
  return 1;
}

static inline void showduino_update_inventory_clear(ShowduinoUpdateInventory *inv) {
  if (!inv) return;
  memset(inv, 0, sizeof(*inv));
}

static inline int showduino_update_inventory_add(ShowduinoUpdateInventory *inv,
                                                 const char *role,
                                                 const char *id,
                                                 const char *name,
                                                 const char *firmware,
                                                 int present,
                                                 int online,
                                                 int healthy,
                                                 int linked,
                                                 int safety_class) {
  ShowduinoUpdateInventoryItem *it;
  if (!inv || !showduino_update_role_ok(role)) return 0;
  if (inv->count >= SHOWDUINO_UPDATE_MAX_INVENTORY) return 0;
  it = &inv->items[inv->count++];
  memset(it, 0, sizeof(*it));
  showduino_update_copy(it->role, sizeof(it->role), role);
  showduino_update_copy(it->id, sizeof(it->id), id && id[0] ? id : role);
  showduino_update_copy(it->name, sizeof(it->name), name);
  showduino_update_copy(it->firmware, sizeof(it->firmware), firmware);
  it->present = present ? 1 : 0;
  it->online = online ? 1 : 0;
  it->healthy = healthy ? 1 : 0;
  it->linked = linked ? 1 : 0;
  it->ota_capable = 0;
  it->safety_class = safety_class ? 1 : 0;
  return 1;
}

/* HEALTHY = booted, NC readable, no local fault. LINKED = fresh heartbeat. */
static inline int showduino_update_station_ready(const ShowduinoUpdateInventoryItem *it) {
  return it && it->present && it->healthy && it->linked;
}

static inline int showduino_update_apply_allowed(int show_running, int emergency_active) {
  (void)show_running;
  (void)emergency_active;
  return 0;
}

static inline const char *showduino_update_apply_block_reason(int show_running,
                                                             int emergency_active) {
  if (emergency_active) return SHOWDUINO_UPDATE_BLOCK_EMERGENCY;
  if (show_running) return SHOWDUINO_UPDATE_BLOCK_SHOW;
  return SHOWDUINO_UPDATE_BLOCK_OTA;
}

/*
 * Authoritative Emergency advance check.
 * Calls showduino_emergency_update_gate() — do not replace this.
 * timed_out + not healthy+linked => SAFETY_NODE_UPDATE_FAILED.
 */
static inline int showduino_update_emergency_advance(ShowduinoEmergencyUpdateGate *g,
                                                     const char *current_id,
                                                     int healthy,
                                                     int linked,
                                                     int timed_out,
                                                     char *fault, size_t fault_cap) {
  if (fault && fault_cap) fault[0] = 0;
  showduino_emergency_update_gate(g, current_id, healthy, linked);
  if (g && g->allow_next) return 1;
  if (timed_out) {
    if (fault && fault_cap) {
      showduino_update_copy(fault, fault_cap, SHOWDUINO_UPDATE_FAULT_SAFETY);
    }
    return 0;
  }
  return 0;
}

static inline void showduino_update_sort_emergency(const ShowduinoUpdateInventory *inv,
                                                   uint8_t *idx, uint8_t *n_out) {
  uint8_t i, j, tmp, n = 0;
  if (!inv || !idx || !n_out) return;
  for (i = 0; i < inv->count && n < SHOWDUINO_UPDATE_MAX_PLAN_STEPS; i++) {
    if (showduino_update_is_emergency_role(inv->items[i].role) && inv->items[i].present) {
      idx[n++] = i;
    }
  }
  for (i = 0; i < n; i++) {
    for (j = (uint8_t)(i + 1); j < n; j++) {
      if (showduino_emergency_id_number(inv->items[idx[j]].id) <
          showduino_emergency_id_number(inv->items[idx[i]].id)) {
        tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
      }
    }
  }
  *n_out = n;
}

static inline void showduino_update_plan_from_inventory(ShowduinoUpdatePlan *plan,
                                                        const ShowduinoUpdateInventory *inv,
                                                        int show_running,
                                                        int emergency_active,
                                                        int safety_timeout) {
  uint8_t i, eidx[SHOWDUINO_UPDATE_MAX_PLAN_STEPS], en = 0, stopped = 0;
  ShowduinoEmergencyUpdateGate gate;
  char fault[40];

  if (!plan) return;
  memset(plan, 0, sizeof(*plan));
  plan->apply_implemented = 0;
  plan->blocked = 1;
  showduino_update_copy(plan->blocked_reason, sizeof(plan->blocked_reason),
                        showduino_update_apply_block_reason(show_running, emergency_active));
  if (!inv) return;

  for (i = 0; i < inv->count && plan->count < SHOWDUINO_UPDATE_MAX_PLAN_STEPS; i++) {
    const ShowduinoUpdateInventoryItem *it = &inv->items[i];
    ShowduinoUpdatePlanStep *st;
    if (showduino_update_is_emergency_role(it->role)) continue;
    st = &plan->steps[plan->count++];
    showduino_update_copy(st->role, sizeof(st->role), it->role);
    showduino_update_copy(st->id, sizeof(st->id), it->id);
    showduino_update_copy(st->state, sizeof(st->state), SHOWDUINO_UPDATE_STEP_READY);
    st->allowed = 1;
    st->one_at_a_time = 0;
  }

  showduino_update_sort_emergency(inv, eidx, &en);
  for (i = 0; i < en && plan->count < SHOWDUINO_UPDATE_MAX_PLAN_STEPS; i++) {
    const ShowduinoUpdateInventoryItem *it = &inv->items[eidx[i]];
    ShowduinoUpdatePlanStep *st = &plan->steps[plan->count++];
    showduino_update_copy(st->role, sizeof(st->role), it->role);
    showduino_update_copy(st->id, sizeof(st->id), it->id);
    st->one_at_a_time = 1;
    if (stopped) {
      showduino_update_copy(st->state, sizeof(st->state),
                            plan->safety_failed ? SHOWDUINO_UPDATE_STEP_FAILED
                                                : SHOWDUINO_UPDATE_STEP_SKIPPED);
      if (plan->safety_failed) {
        showduino_update_copy(st->fault, sizeof(st->fault), SHOWDUINO_UPDATE_FAULT_SAFETY);
      }
      continue;
    }
    if (i == 0) {
      st->allowed = 1;
      showduino_update_copy(st->state, sizeof(st->state), SHOWDUINO_UPDATE_STEP_READY);
      continue;
    }
    fault[0] = 0;
    {
      const ShowduinoUpdateInventoryItem *prev = &inv->items[eidx[i - 1]];
      if (showduino_update_emergency_advance(&gate, prev->id, prev->healthy, prev->linked,
                                             safety_timeout, fault, sizeof(fault))) {
        st->allowed = 1;
        showduino_update_copy(st->state, sizeof(st->state), SHOWDUINO_UPDATE_STEP_READY);
      } else if (fault[0]) {
        plan->safety_failed = 1;
        stopped = 1;
        st->allowed = 0;
        showduino_update_copy(st->state, sizeof(st->state), SHOWDUINO_UPDATE_STEP_FAILED);
        showduino_update_copy(st->fault, sizeof(st->fault), fault);
        showduino_update_copy(plan->blocked_reason, sizeof(plan->blocked_reason),
                              SHOWDUINO_UPDATE_FAULT_SAFETY);
      } else {
        stopped = 1;
        st->allowed = 0;
        showduino_update_copy(st->state, sizeof(st->state), SHOWDUINO_UPDATE_STEP_HOLD);
        showduino_update_copy(st->hold_reason, sizeof(st->hold_reason),
                              gate.hold_reason[0] ? gate.hold_reason : SHOWDUINO_UPDATE_BLOCK_GATE);
      }
    }
  }
}

static inline int showduino_ota_sha256_hex_ok(const char *hex) {
  size_t i;
  if (!hex) return 0;
  for (i = 0; i < SHOWDUINO_OTA_SHA256_HEX_LEN; i++) {
    char c = hex[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return 0;
    }
  }
  return hex[SHOWDUINO_OTA_SHA256_HEX_LEN] == 0;
}

static inline int showduino_ota_hardware_match(const char *want, const char *have) {
  return want && have && strcmp(want, have) == 0;
}

static inline int showduino_update_component_ota_available(const char *role) {
  return showduino_update_is_comms_role(role);
}

typedef struct ShowduinoCommsHealth {
  uint8_t booted;
  uint8_t uart;
  uint8_t p4_link;
  uint8_t espnow;
  uint8_t network;
  uint8_t webui;
  uint8_t fatal;
} ShowduinoCommsHealth;

static inline int showduino_comms_health_pass(const ShowduinoCommsHealth *h) {
  if (!h) return 0;
  if (h->fatal) return 0;
  return h->booted && h->uart && h->p4_link && h->espnow && h->network && h->webui;
}

typedef struct ShowduinoOtaCandidate {
  char role[SHOWDUINO_UPDATE_ROLE_MAX + 1];
  char hardware_id[32];
  char firmware[SHOWDUINO_UPDATE_FW_MAX + 1];
  char filename[48];
  char sha256[SHOWDUINO_OTA_SHA256_HEX_LEN + 1];
  uint32_t size;
  uint8_t ota_capable;
  uint8_t force;
  uint8_t confirm;
} ShowduinoOtaCandidate;

static inline const char *showduino_comms_ota_reject_reason(
    const ShowduinoOtaCandidate *c,
    const char *installed_ver,
    const char *local_hardware,
    int show_running,
    int emergency_active,
    int maintenance,
    int p4_alive) {
  int cmp;
  if (!c) return SHOWDUINO_UPDATE_BLOCK_MANIFEST;
  if (!showduino_update_is_comms_role(c->role)) {
    if (showduino_update_role_ok(c->role)) return SHOWDUINO_UPDATE_BLOCK_COMPONENT;
    return SHOWDUINO_UPDATE_BLOCK_ROLE;
  }
  if (!c->ota_capable) return SHOWDUINO_UPDATE_BLOCK_COMPONENT;
  if (!showduino_ota_hardware_match(c->hardware_id, local_hardware)) {
    return SHOWDUINO_UPDATE_BLOCK_HARDWARE;
  }
  if (!c->firmware[0] || !installed_ver) return SHOWDUINO_UPDATE_BLOCK_MANIFEST;
  if (!showduino_ota_sha256_hex_ok(c->sha256)) return SHOWDUINO_UPDATE_BLOCK_SHA;
  if (c->size == 0 || c->size > SHOWDUINO_COMMS_OTA_SLOT_BYTES) {
    return SHOWDUINO_UPDATE_BLOCK_SIZE;
  }
  if (!c->confirm) return SHOWDUINO_UPDATE_BLOCK_CONFIRM;
  if (!p4_alive) return SHOWDUINO_UPDATE_BLOCK_P4;
  if (emergency_active) return SHOWDUINO_UPDATE_BLOCK_EMERGENCY;
  if (show_running) return SHOWDUINO_UPDATE_BLOCK_SHOW;
  if (!maintenance) return SHOWDUINO_UPDATE_BLOCK_MAINT;
  cmp = showduino_version_compare(c->firmware, installed_ver);
  if (cmp < 0) return SHOWDUINO_UPDATE_BLOCK_DOWNGRADE;
  if (cmp == 0 && !c->force) return SHOWDUINO_UPDATE_BLOCK_SAME;
  return NULL;
}

/* Discovery eligibility: same as apply, without live show/maintenance/confirm. */
static inline const char *showduino_comms_discover_reason(
    const ShowduinoOtaCandidate *c,
    const char *installed_ver,
    const char *local_hardware) {
  int cmp;
  if (!c) return SHOWDUINO_UPDATE_BLOCK_MANIFEST;
  if (!showduino_update_is_comms_role(c->role)) {
    if (showduino_update_role_ok(c->role)) return SHOWDUINO_UPDATE_BLOCK_COMPONENT;
    return SHOWDUINO_UPDATE_BLOCK_ROLE;
  }
  if (!c->ota_capable) return SHOWDUINO_UPDATE_BLOCK_COMPONENT;
  if (!showduino_ota_hardware_match(c->hardware_id, local_hardware)) {
    return SHOWDUINO_UPDATE_BLOCK_HARDWARE;
  }
  if (!c->firmware[0] || !installed_ver) return SHOWDUINO_UPDATE_BLOCK_MANIFEST;
  if (!showduino_ota_sha256_hex_ok(c->sha256)) return SHOWDUINO_UPDATE_BLOCK_SHA;
  if (c->size == 0 || c->size > SHOWDUINO_COMMS_OTA_SLOT_BYTES) {
    return SHOWDUINO_UPDATE_BLOCK_SIZE;
  }
  cmp = showduino_version_compare(c->firmware, installed_ver);
  if (cmp < 0) return SHOWDUINO_UPDATE_BLOCK_DOWNGRADE;
  if (cmp == 0) return SHOWDUINO_UPDATE_BLOCK_SAME;
  return NULL;
}

static inline const char *showduino_ota_state_after(const char *state, int event) {
  if (!state) return SHOWDUINO_OTA_STATE_FAILED;
  if (event == SHOWDUINO_OTA_EVT_EMERGENCY) return SHOWDUINO_OTA_STATE_EMERGENCY;
  if (strcmp(state, SHOWDUINO_OTA_STATE_IDLE) == 0 &&
      event == SHOWDUINO_OTA_EVT_START) {
    return SHOWDUINO_OTA_STATE_DOWNLOADING;
  }
  if (strcmp(state, SHOWDUINO_OTA_STATE_DOWNLOADING) == 0) {
    if (event == SHOWDUINO_OTA_EVT_DOWNLOAD_OK) return SHOWDUINO_OTA_STATE_VERIFYING;
    if (event == SHOWDUINO_OTA_EVT_DOWNLOAD_FAIL) return SHOWDUINO_OTA_STATE_FAILED;
  }
  if (strcmp(state, SHOWDUINO_OTA_STATE_VERIFYING) == 0) {
    if (event == SHOWDUINO_OTA_EVT_HASH_OK) return SHOWDUINO_OTA_STATE_INSTALLING;
    if (event == SHOWDUINO_OTA_EVT_HASH_FAIL) return SHOWDUINO_OTA_STATE_FAILED;
  }
  if (strcmp(state, SHOWDUINO_OTA_STATE_INSTALLING) == 0) {
    if (event == SHOWDUINO_OTA_EVT_WRITE_OK) return SHOWDUINO_OTA_STATE_REBOOT;
    if (event == SHOWDUINO_OTA_EVT_WRITE_FAIL) return SHOWDUINO_OTA_STATE_FAILED;
  }
  if (strcmp(state, SHOWDUINO_OTA_STATE_REBOOT) == 0 &&
      event == SHOWDUINO_OTA_EVT_REBOOTED) {
    return SHOWDUINO_OTA_STATE_PENDING;
  }
  if (strcmp(state, SHOWDUINO_OTA_STATE_PENDING) == 0) {
    if (event == SHOWDUINO_OTA_EVT_HEALTH_OK) return SHOWDUINO_OTA_STATE_COMPLETE;
    if (event == SHOWDUINO_OTA_EVT_HEALTH_FAIL) return SHOWDUINO_OTA_STATE_FAILED;
  }
  if (event == SHOWDUINO_OTA_EVT_ROLLBACK_DONE) return SHOWDUINO_OTA_STATE_ROLLED_BACK;
  return state;
}

#ifdef __cplusplus
}
#endif

#endif /* SHOWDUINO_UPDATE_MANAGER_H */
