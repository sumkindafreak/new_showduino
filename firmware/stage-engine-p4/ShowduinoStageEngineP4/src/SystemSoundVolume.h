#pragma once
#include <stdint.h>
inline uint8_t systemSoundOutputVolume(uint8_t volume, bool emergency) {
  return emergency ? 80 : (volume > 100 ? 80 : volume);
}
inline bool parseSystemSoundVolume(const char *text, uint8_t *value) {
  if (!text || !*text || !value) return false;
  unsigned n = 0;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9') return false;
    n = n * 10 + unsigned(*text - '0');
    if (n > 100) return false;
  }
  *value = (uint8_t)n;
  return true;
}
