#include "EmergencyLatchStore.h"
#include <Preferences.h>
#include "showduino_emergency_latch.h"

static ShowduinoLatchTrack sTrack = {false, true, false};
static ShowduinoLatchRead sBootRead = SHOWDUINO_LATCH_READ_FAULT;

/* The namespace is opened read-write on purpose. Opening it read-only fails
 * when the namespace has never been written, which would look like a storage
 * fault and latch every new device on first boot. */
static ShowduinoLatchRead readStore() {
  ShowduinoLatchReadInput in = {};
  Preferences p;
  in.storeOpened = p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false);
  if (in.storeOpened) {
    in.readOk = true;
    in.keyPresent = p.isKey(SHOWDUINO_LATCH_NVS_KEY);
    if (in.keyPresent) in.value = p.getUChar(SHOWDUINO_LATCH_NVS_KEY, 0);
    p.end();
  }
  return showduino_latch_decode(in);
}

static bool writeStore(bool active, bool blockersVerified) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const uint8_t v = showduino_latch_value_for(active, blockersVerified);
  bool ok = (p.putUChar(SHOWDUINO_LATCH_NVS_KEY, v) == sizeof(uint8_t));
  if (ok) ok = (p.getUChar(SHOWDUINO_LATCH_NVS_KEY, 0) == v); /* read-back verify */
  p.end();
  return ok;
}

bool emergencyLatchRestoreAtBoot() {
  sBootRead = readStore();
  showduino_latch_track_from_boot(&sTrack, sBootRead);
  const bool latched = showduino_latch_boot_latched(sBootRead);
  switch (sBootRead) {
    case SHOWDUINO_LATCH_READ_ACTIVE:
      Serial.println("[ESTOP] Stored latch ACTIVE (blockers verified) — Emergency restored");
      break;
    case SHOWDUINO_LATCH_READ_ACTIVE_UNVERIFIED:
      Serial.println("[ESTOP] Stored latch ACTIVE with UNVERIFIED blocker record — clear blocked");
      break;
    case SHOWDUINO_LATCH_READ_CLEAR:
      Serial.println("[ESTOP] Stored latch CLEAR");
      break;
    default:
      Serial.println("[ESTOP] WARNING: latch storage unreadable or corrupt — fail-safe LATCHED, clear blocked");
      break;
  }
  if (latched && sTrack.dirty) {
    /* Converge storage to ACTIVE. Blocker state is not yet known, so store it unverified. */
    const bool ok = writeStore(true, false);
    showduino_latch_record_write_v2(&sTrack, true, false, ok);
    Serial.println(ok ? "[ESTOP] Latch ACTIVE (unverified) written to storage"
                      : "[ESTOP] WARNING: could not write latch to storage — RAM latch still active");
  }
  return latched;
}

bool emergencyLatchBootUnknown() {
  return showduino_latch_boot_unknown(sBootRead);
}

bool emergencyLatchSetActive(bool blockersVerified) {
  if (!showduino_latch_needs_write_v2(&sTrack, true, blockersVerified)) return true;
  const bool ok = writeStore(true, blockersVerified);
  showduino_latch_record_write_v2(&sTrack, true, blockersVerified, ok);
  if (!ok) {
    Serial.println("[ESTOP] WARNING: latch ACTIVE could not be verified in storage");
  }
  return ok;
}

bool emergencyLatchRequest(bool active) {
  if (active) return emergencyLatchSetActive(true);
  if (!showduino_latch_needs_write_v2(&sTrack, false, true)) return true;
  const bool ok = writeStore(false, true);
  showduino_latch_record_write_v2(&sTrack, false, true, ok);
  if (!ok) {
    Serial.println("[ESTOP] WARNING: latch CLEAR could not be verified in storage");
  }
  return ok;
}

bool emergencyLatchNeedsReconcile(bool ramActive, bool blockersVerified) {
  return showduino_latch_needs_write_v2(&sTrack, ramActive, blockersVerified);
}

void emergencyLatchReconcile(bool ramActive, bool blockersVerified) {
  if (!emergencyLatchNeedsReconcile(ramActive, blockersVerified)) return;
  if (ramActive) emergencyLatchSetActive(blockersVerified);
  else emergencyLatchRequest(false);
}
