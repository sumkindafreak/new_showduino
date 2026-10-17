#ifndef SHOWDUINO_EMERGENCY_ENGAGE_H
#define SHOWDUINO_EMERGENCY_ENGAGE_H

/*
 * Emergency actuation order (MISSION 001F).
 *
 *   1. engageRam: latch in RAM and every emergency-safe local output. This
 *      step runs first, whatever the storage state, and never waits on storage.
 *   2. persistBlockers: wireless clear-blocker record. Returns 1 when verified.
 *   3. persistLatch: latch byte, told whether the blocker record verified.
 *      Returns 1 when verified.
 *
 * A storage failure or delay after step 1 cannot undo or delay step 1. A latch
 * that is not durably stored stays active in RAM and is retried by the caller.
 * Returns 1 only when both records are verified.
 *
 * Limit: an assertion that never reaches storage cannot survive a P4 reboot.
 * Software cannot recover it. See docs/firmware-p4-emergency-latch.md.
 */

typedef struct ShowduinoEngageOps {
  void *ctx;
  void (*engageRam)(void *ctx);
  int (*persistBlockers)(void *ctx);
  int (*persistLatch)(void *ctx, int blockersVerified);
} ShowduinoEngageOps;

static inline int showduino_emergency_engage(const ShowduinoEngageOps *ops) {
  if (!ops || !ops->engageRam || !ops->persistBlockers || !ops->persistLatch) return 0;
  ops->engageRam(ops->ctx);
  const int blockersVerified = ops->persistBlockers(ops->ctx) ? 1 : 0;
  const int latchVerified = ops->persistLatch(ops->ctx, blockersVerified) ? 1 : 0;
  return (blockersVerified && latchVerified) ? 1 : 0;
}

#endif
