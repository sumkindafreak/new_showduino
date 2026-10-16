#ifndef SHOWDUINO_EMERGENCY_LATCH_STORE_H
#define SHOWDUINO_EMERGENCY_LATCH_STORE_H

/*
 * NVS-backed persistence for the P4 emergency latch.
 * Policy lives in protocol/showduino_emergency_latch.h. This file performs
 * the Preferences (NVS) calls. The byte is written only on latch transitions.
 */

#include <Arduino.h>

/* Called once at boot, before any output, show or node link starts.
 * Returns true when the RAM latch must be engaged. A storage fault returns
 * true (fail-safe). */
bool emergencyLatchRestoreAtBoot();

/* Persist the desired state. Returns true when storage now matches (or no
 * write was needed). For a CLEAR, false must refuse the clear. */
bool emergencyLatchRequest(bool active);

/* Retry a previously failed write, if any. ramActive is the RAM latch state,
 * which is what storage must converge to. */
void emergencyLatchRetryIfDirty(bool ramActive);

/* True while storage is known not to hold the latched state. */
bool emergencyLatchStorageFault();

#endif
