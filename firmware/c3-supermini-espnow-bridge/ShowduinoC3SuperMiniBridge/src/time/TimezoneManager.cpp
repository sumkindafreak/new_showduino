#include "TimezoneManager.h"
#include <string.h>

void TimezoneManager::begin(const char *tzName, bool dstEnabled) {
  strncpy(tzName_, tzName && tzName[0] ? tzName : "UTC", sizeof(tzName_) - 1);
  tzName_[sizeof(tzName_) - 1] = '\0';
  dstEnabled_ = dstEnabled;
  dstActive_ = false;
  utcOffsetSec_ = 0; /* Stage 7.5: UTC canonical; offsets later */
}

uint32_t TimezoneManager::toLocal(uint32_t utcEpoch) const {
  return (uint32_t)((int64_t)utcEpoch + utcOffsetSec_);
}

uint32_t TimezoneManager::toUtc(uint32_t localEpoch) const {
  return (uint32_t)((int64_t)localEpoch - utcOffsetSec_);
}