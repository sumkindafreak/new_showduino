#ifndef SHOWDUINO_EMERGENCY_LATCH_STORE_H
#define SHOWDUINO_EMERGENCY_LATCH_STORE_H

/*
 * NVS-backed persistence for the P4 emergency latch.
 * Policy lives in protocol/showduino_emergency_latch.h. The stored byte is
 * 0xA5 (ACTIVE, blocker record verified), 0xA6 (ACTIVE, blocker record NOT
 * verified) or 0x5A (CLEAR). Written only on transitions, with read-back.
 */

#include <Arduino.h>

/* Called once at boot, before any output, show or node link starts.
 * Returns true when the RAM latch must be engaged. A storage fault returns
 * true (fail-safe). */
bool emergencyLatchRestoreAtBoot();

/* True when the boot read was FAULT or ACTIVE_UNVERIFIED. The safety state is
 * then unknown and clear must be refused until a service action. */
bool emergencyLatchBootUnknown();

/* Persist ACTIVE. blockersVerified=false stores the unverified value. Returns
 * true when storage now matches. */
bool emergencyLatchSetActive(bool blockersVerified);

/* Persist CLEAR. Returns true only when the CLEAR is verified. */
bool emergencyLatchRequest(bool active);

/* True when storage does not yet match the RAM latch and blocker verification. */
bool emergencyLatchNeedsReconcile(bool ramActive, bool blockersVerified);

/* Bring storage to match. Writes only when needed. */
void emergencyLatchReconcile(bool ramActive, bool blockersVerified);

#endif
