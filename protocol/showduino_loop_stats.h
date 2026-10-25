#ifndef SHOWDUINO_LOOP_STATS_H
#define SHOWDUINO_LOOP_STATS_H

/*
 * MISSION 001L: main-loop duration statistics for bench timing.
 *
 * The P4 samples GPIO25 once per loop() (with a 30 ms debounce), so the time from
 * a physical press to detection depends on the slowest loop iterations. This keeps
 * a count per bucket and the worst value so the owner can read the real
 * distribution with DIAG:LOOP on the P4 USB console. Measurement only: it changes
 * no behaviour.
 */

#include <stdint.h>
#include <string.h>

#define SHOWDUINO_LOOP_BUCKETS 6
/* Upper bounds in microseconds; the last bucket is everything above 100 ms. */
static const uint32_t kShowduinoLoopBucketUs[SHOWDUINO_LOOP_BUCKETS - 1] = {1000u, 5000u, 20000u,
                                                                              50000u, 100000u};

typedef struct ShowduinoLoopStats {
  uint32_t count;
  uint32_t maxUs;
  uint32_t lastUs;
  uint64_t totalUs;
  uint32_t bucket[SHOWDUINO_LOOP_BUCKETS];
} ShowduinoLoopStats;

static inline void showduino_loop_stats_reset(ShowduinoLoopStats *s) {
  if (s) memset(s, 0, sizeof(*s));
}

static inline int showduino_loop_bucket_for(uint32_t us) {
  for (int i = 0; i < SHOWDUINO_LOOP_BUCKETS - 1; ++i) {
    if (us <= kShowduinoLoopBucketUs[i]) return i;
  }
  return SHOWDUINO_LOOP_BUCKETS - 1;
}

static inline void showduino_loop_stats_add(ShowduinoLoopStats *s, uint32_t us) {
  if (!s) return;
  if (s->count != 0xFFFFFFFFu) s->count++;
  s->lastUs = us;
  s->totalUs += us;
  if (us > s->maxUs) s->maxUs = us;
  uint32_t *b = &s->bucket[showduino_loop_bucket_for(us)];
  if (*b != 0xFFFFFFFFu) (*b)++;
}

#endif /* SHOWDUINO_LOOP_STATS_H */
