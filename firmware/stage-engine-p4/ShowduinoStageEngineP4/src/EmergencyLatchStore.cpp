#include "EmergencyLatchStore.h"
#include <Preferences.h>
#include "../../../../protocol/showduino_emergency_latch.h"

static ShowduinoLatchTrack sTrack = {false, true};

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

static bool writeStore(bool active) {
  Preferences p;
  if (!p.begin(SHOWDUINO_LATCH_NVS_NAMESPACE, false)) return false;
  const uint8_t v = active ? SHOWDUINO_LATCH_VALUE_ACTIVE : SHOWDUINO_LATCH_VALUE_CLEAR;
  bool ok = (p.putUChar(SHOWDUINO_LATCH_NVS_KEY, v) == sizeof(uint8_t));
  if (ok) ok = (p.getUChar(SHOWDUINO_LATCH_NVS_KEY, 0) == v); /* read-back verify */
  p.end();
  return ok;
}

bool emergencyLatchRestoreAtBoot() {
  const ShowduinoLatchRead r = readStore();
  showduino_latch_track_from_boot(&sTrack, r);
  const bool latched = showduino_latch_boot_latched(r);
  switch (r) {
    case SHOWDUINO_LATCH_READ_ACTIVE:
      Serial.println("[ESTOP] Stored latch ACTIVE — Emergency restored before show start");
      break;
    case SHOWDUINO_LATCH_READ_CLEAR:
      Serial.println("[ESTOP] Stored latch CLEAR");
      break;
    default:
      Serial.println("[ESTOP] WARNING: latch storage unreadable or corrupt — fail-safe LATCHED");
      break;
  }
  if (latched && sTrack.dirty) {
    /* Converge storage to ACTIVE now that the RAM latch is engaged. */
    const bool ok = writeStore(true);
    showduino_latch_record_write(&sTrack, true, ok);
    Serial.println(ok ? "[ESTOP] Latch ACTIVE written to storage"
                      : "[ESTOP] WARNING: could not write latch to storage — RAM latch still active");
  }
  return latched;
}

bool emergencyLatchRequest(bool active) {
  if (!showduino_latch_needs_write(&sTrack, active)) return true;
  const bool ok = writeStore(active);
  showduino_latch_record_write(&sTrack, active, ok);
  if (!ok) {
    Serial.printf("[ESTOP] WARNING: latch %s could not be verified in storage\n",
                  active ? "ACTIVE" : "CLEAR");
  }
  return ok;
}

void emergencyLatchRetryIfDirty(bool ramActive) {
  /* Follows the RAM latch, never the last request, so a refused clear cannot
   * write CLEAR while the emergency is still engaged. */
  if (sTrack.dirty) emergencyLatchRequest(ramActive);
}

bool emergencyLatchStorageFault() {
  return sTrack.dirty;
}
