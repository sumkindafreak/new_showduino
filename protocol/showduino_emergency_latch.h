#ifndef SHOWDUINO_EMERGENCY_LATCH_H
#define SHOWDUINO_EMERGENCY_LATCH_H

/*
 * Persistent emergency-latch policy for the ESP32-P4 Show Engine.
 *
 * Pure logic only: no Arduino, ESP-IDF or GPIO dependency, so the decisions
 * can be exercised by host tests (tools/emergency-latch-tests). The P4 wrapper
 * (firmware/stage-engine-p4/.../src/EmergencyLatchStore.cpp) performs the NVS
 * calls and feeds their results into these functions.
 *
 * Storage: one byte in NVS namespace "showduino", key "estop_latch".
 *   0xA5 = latched (Emergency active)
 *   0x5A = clear
 *   absent key on a readable store = clear (first boot / never latched)
 *   any other value, a failed open, or a failed read = FAULT
 *
 * Rules enforced here:
 *   - FAULT at boot is treated as LATCHED (fail-safe). Storage failure must
 *     never silently release a known or possible emergency.
 *   - The stored value changes only on latch transitions, never per loop.
 *   - A CLEAR is released from RAM only after the CLEAR byte is written and
 *     read back. If the write cannot be verified the latch stays active.
 *   - The physical button is not an input to the boot decision. A latch
 *     persisted while the button was pressed stays latched after release.
 */

#include <stdint.h>

#define SHOWDUINO_LATCH_NVS_NAMESPACE "showduino"
#define SHOWDUINO_LATCH_NVS_KEY       "estop_latch"
#define SHOWDUINO_LATCH_VALUE_ACTIVE  0xA5u
#define SHOWDUINO_LATCH_VALUE_CLEAR   0x5Au

enum ShowduinoLatchRead : uint8_t {
  SHOWDUINO_LATCH_READ_CLEAR  = 0,
  SHOWDUINO_LATCH_READ_ACTIVE = 1,
  SHOWDUINO_LATCH_READ_FAULT  = 2
};

struct ShowduinoLatchReadInput {
  bool    storeOpened; /* NVS namespace opened */
  bool    readOk;      /* read call completed without error */
  bool    keyPresent;  /* key exists in the namespace */
  uint8_t value;       /* stored byte (valid only when keyPresent) */
};

struct ShowduinoLatchTrack {
  bool durableActive; /* last value confirmed in storage */
  bool dirty;         /* storage does not yet hold the desired state */
};

inline ShowduinoLatchRead showduino_latch_decode(const ShowduinoLatchReadInput &in) {
  if (!in.storeOpened || !in.readOk) return SHOWDUINO_LATCH_READ_FAULT;
  if (!in.keyPresent) return SHOWDUINO_LATCH_READ_CLEAR;
  if (in.value == SHOWDUINO_LATCH_VALUE_ACTIVE) return SHOWDUINO_LATCH_READ_ACTIVE;
  if (in.value == SHOWDUINO_LATCH_VALUE_CLEAR)  return SHOWDUINO_LATCH_READ_CLEAR;
  return SHOWDUINO_LATCH_READ_FAULT;
}

/* Boot decision: FAULT latches, as does a stored ACTIVE. */
inline bool showduino_latch_boot_latched(ShowduinoLatchRead r) {
  return r != SHOWDUINO_LATCH_READ_CLEAR;
}

/* Initialise the tracker from the boot read. FAULT means the stored state is
 * unknown, so the desired ACTIVE state is marked dirty and written on the next
 * opportunity. */
inline void showduino_latch_track_from_boot(ShowduinoLatchTrack *t, ShowduinoLatchRead r) {
  switch (r) {
    case SHOWDUINO_LATCH_READ_ACTIVE:
      t->durableActive = true;  t->dirty = false; break;
    case SHOWDUINO_LATCH_READ_CLEAR:
      t->durableActive = false; t->dirty = false; break;
    default:
      t->durableActive = false; t->dirty = true;  break;
  }
}

/* True when storage must be written to reach desiredActive. */
inline bool showduino_latch_needs_write(const ShowduinoLatchTrack *t, bool desiredActive) {
  return t->dirty || (t->durableActive != desiredActive);
}

/* Record a write attempt. verified = write returned success and read-back matched. */
inline void showduino_latch_record_write(ShowduinoLatchTrack *t, bool desiredActive, bool verified) {
  if (verified) {
    t->durableActive = desiredActive;
    t->dirty = false;
  } else {
    t->dirty = true;
  }
}

/* A CLEAR may release the RAM latch only after the CLEAR is durable. */
inline bool showduino_latch_clear_may_release(bool clearWriteVerified) {
  return clearWriteVerified;
}

/* Wireless / station gate for a Director or USB clear.
 * `pressed_stations` is the number of Emergency Nodes whose most recent report
 * says their button is still pressed (input_open=1). A pressed station blocks
 * clear. A station that was pressed and has since gone silent also blocks,
 * because its state is unknown. Returns non-zero when clear must be refused. */
static inline int showduino_station_clear_blocked(unsigned pressed_stations) {
  return pressed_stations != 0u;
}

#endif
