#ifndef SHOWDUINO_EMERGENCY_STATION_KEYS_H
#define SHOWDUINO_EMERGENCY_STATION_KEYS_H

/*
 * MISSION 001J: P4 store for Emergency Node station keys and the command/episode
 * counter-block limit.
 *
 * Keys are 32-byte secrets, one per station ID, entered on the P4 USB console
 * only (EMERGENCY:KEY:SET). They are stored in NVS in plaintext: this board has
 * no flash encryption enabled, so anyone with physical access to the P4 flash can
 * read them. See docs/emergency-station-auth-001J.md.
 *
 * Every write is read back and compared. A stored record that exists but cannot
 * be read or fails its check sets stationKeysFault(), and the Director clear stays
 * refused until it is repaired on the USB console.
 */

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_station_auth.h"
#include "../../../protocol/showduino_director_auth.h"

#define SHOWDUINO_STATION_KEYS_MAX SHOWDUINO_EMERGENCY_NODE_MAX_NODES

void stationKeysBegin();
bool stationKeysFault();
uint8_t stationKeysCount();
/* ID of key slot i, or "" when i is out of range. */
const char *stationKeysId(uint8_t i);
/* Copies the key for `id` into out and returns its slot, or -1 (out zeroed). */
int stationKeysLookup(const char *id, uint8_t out[SHOWDUINO_AUTH_KEY_BYTES]);
/* Adds or replaces. Returns false (and changes nothing in RAM) if the verified
 * write fails or the table is full. */
bool stationKeysSet(const char *id, const uint8_t key[SHOWDUINO_AUTH_KEY_BYTES]);
bool stationKeysRevoke(const char *id);
/* Discards an unreadable record so keys can be entered again. USB console only. */
bool stationKeysReset();
/* First 4 bytes of HMAC(key, "SHOWDUINO-KEY-FP|v1") as hex, for matching a P4 slot
 * to a station without printing the key. Empty when the ID has no key. */
void stationKeysFingerprint(const char *id, char out[9]);

/* ---- MISSION 001K: enrolled Director credential (operator clear authority) ----
 * NVS: estop_dkey (record + CRC32), estop_dctr (last accepted Director counter).
 * One Director at a time. A key equal to any station key is refused (and a station
 * key equal to the Director key), so station and operator authority never share a
 * secret. Enrolling while latched marks the Director restricted until a clear is
 * accepted through the USB maintenance authority. */
bool directorKeyFault();
/* Fills rec from RAM. present=0 when no Director is enrolled or on a fault. */
void directorKeyGet(ShowduinoDirKeyRec *rec);
bool directorKeySet(const char *id, const uint8_t key[SHOWDUINO_AUTH_KEY_BYTES], bool restricted);
bool directorKeyRevoke();
bool directorKeyClearRestriction();
/* Persists the last accepted Director counter (verified write). */
bool directorCounterSave(uint32_t value);
void directorKeyFingerprint(char out[9]);
bool directorKeyReset();

/* Counter-block limit (shared by commands and episode ids). */
bool stationCounterLoad(uint32_t *limit);
bool stationCounterSave(uint32_t limit);

#endif
