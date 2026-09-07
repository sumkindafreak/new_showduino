#include "StageTime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <Arduino.h>
#include "../BoardConfig.h"

#ifndef SHOWDUINO_RTC_SYNC_FLOOR
#define SHOWDUINO_RTC_SYNC_FLOOR 1600000000UL
#endif
#ifndef SHOWDUINO_RTC_PUBLISH_MS
#define SHOWDUINO_RTC_PUBLISH_MS 1000UL
#endif
#ifndef SHOWDUINO_RTC_MAX_EPOCH
#define SHOWDUINO_RTC_MAX_EPOCH 4102444799UL
#endif

enum class ClockSource : uint8_t {
  None = 0,
  CompileSeed,
  Rtc
};

static ClockSource sSource = ClockSource::None;
static bool sSynced = false;
static uint32_t sLastPublishMs = 0;

static const char *kDow[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *kMon[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void explodeEpoch(uint32_t epoch, int &Y, int &M, int &D, int &h, int &m, int &s) {
  uint32_t z = epoch / 86400UL + 719468UL;
  uint32_t era = z / 146097UL;
  uint32_t doe = z - era * 146097UL;
  uint32_t yoe = (doe - doe / 1460UL + doe / 36524UL - doe / 146096UL) / 365UL;
  uint32_t y = yoe + era * 400UL;
  uint32_t doy = doe - (365UL * yoe + yoe / 4UL - yoe / 100UL);
  uint32_t mp = (5UL * doy + 2UL) / 153UL;
  D = (int)(doy - (153UL * mp + 2UL) / 5UL + 1UL);
  M = (int)(mp < 10 ? mp + 3 : mp - 9);
  Y = (int)(y + (M <= 2 ? 1 : 0));
  uint32_t tod = epoch % 86400UL;
  h = (int)(tod / 3600UL);
  m = (int)((tod % 3600UL) / 60UL);
  s = (int)(tod % 60UL);
}

static uint32_t civilToEpoch(int Y, int M, int D, int h, int mi, int s) {
  Y -= (M <= 2);
  const int era = (Y >= 0 ? Y : Y - 399) / 400;
  const unsigned yoe = (unsigned)(Y - era * 400);
  const unsigned doy = (unsigned)((153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1);
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
  return (uint32_t)(days * 86400LL + h * 3600 + mi * 60 + s);
}

static int monthFromName(const char *name) {
  for (int i = 0; i < 12; i++) {
    if (strncmp(name, kMon[i], 3) == 0) return i + 1;
  }
  return 0;
}

static uint32_t compileEpoch() {
  char mon[5] = {};
  int D = 0, Y = 0, h = 0, mi = 0, s = 0;
  if (sscanf(__DATE__, "%3s %d %d", mon, &D, &Y) != 3) return 0;
  if (sscanf(__TIME__, "%d:%d:%d", &h, &mi, &s) != 3) return 0;
  const int M = monthFromName(mon);
  if (M < 1 || D < 1 || Y < 2020) return 0;
  return civilToEpoch(Y, M, D, h, mi, s);
}

static bool writeRtc(uint32_t epoch) {
  struct timeval tv = {};
  tv.tv_sec = (time_t)epoch;
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

static uint32_t readRtc() {
  struct timeval tv = {};
  if (gettimeofday(&tv, nullptr) != 0) return 0;
  if (tv.tv_sec <= 0) return 0;
  return (uint32_t)tv.tv_sec;
}

static bool epochInRange(uint32_t epoch) {
  return epoch >= SHOWDUINO_RTC_SYNC_FLOOR && epoch <= SHOWDUINO_RTC_MAX_EPOCH;
}

void stageTimeBegin() {
  setenv("TZ", "UTC0", 1);
  tzset();

  const uint32_t now = readRtc();
  if (epochInRange(now)) {
    sSource = ClockSource::Rtc;
    sSynced = true;
    Serial.printf("[RTC] Internal RTC retained %lu\n", (unsigned long)now);
    return;
  }

  const uint32_t seed = compileEpoch();
  if (seed >= SHOWDUINO_RTC_SYNC_FLOOR && writeRtc(seed)) {
    sSource = ClockSource::CompileSeed;
    sSynced = false;
    Serial.printf("[RTC] Internal RTC seeded from firmware build %lu (unsynced)\n",
                  (unsigned long)seed);
    return;
  }

  sSource = ClockSource::None;
  sSynced = false;
  Serial.println("[RTC] Internal RTC not available");
}

uint32_t stageTimeEpoch() {
  return readRtc();
}

bool stageTimeSynced() {
  return sSynced && epochInRange(readRtc());
}

const char *stageTimeHealth() {
  if (sSource == ClockSource::None) return "offline";
  return stageTimeSynced() ? "healthy" : "unsynced";
}

const char *stageTimeSource() {
  switch (sSource) {
    case ClockSource::Rtc: return "rtc";
    case ClockSource::CompileSeed: return "compile-seed";
    default: return "none";
  }
}

void stageTimeIso(char *out, size_t outLen) {
  if (!out || outLen < 21) return;
  int Y, M, D, h, m, s;
  explodeEpoch(readRtc(), Y, M, D, h, m, s);
  snprintf(out, outLen, "%04d-%02d-%02dT%02d:%02d:%02dZ", Y, M, D, h, m, s);
}

void stageTimeClock(char *out, size_t outLen) {
  if (!out || outLen < 9) return;
  int Y, M, D, h, m, s;
  explodeEpoch(readRtc(), Y, M, D, h, m, s);
  (void)Y; (void)M; (void)D;
  snprintf(out, outLen, "%02d:%02d:%02d", h, m, s);
}

void stageTimeDate(char *out, size_t outLen) {
  if (!out || outLen < 11) return;
  int Y, M, D, h, m, s;
  explodeEpoch(readRtc(), Y, M, D, h, m, s);
  (void)h; (void)m; (void)s;
  snprintf(out, outLen, "%04d-%02d-%02d", Y, M, D);
}

void stageTimeLongDate(char *out, size_t outLen) {
  if (!out || outLen < 16) return;
  int Y, M, D, h, m, s;
  const uint32_t epoch = readRtc();
  explodeEpoch(epoch, Y, M, D, h, m, s);
  (void)h; (void)m; (void)s;
  const char *dow = kDow[(epoch / 86400UL + 4UL) % 7UL];
  const char *mon = (M >= 1 && M <= 12) ? kMon[M - 1] : "???";
  snprintf(out, outLen, "%s %d %s %04d", dow, D, mon, Y);
}

bool stageTimeFormatDirectorWire(char *out, size_t outLen) {
  if (!out || outLen < 40) return false;
  const uint32_t epoch = readRtc();
  char tod[10];
  char longDate[24];
  stageTimeClock(tod, sizeof(tod));
  stageTimeLongDate(longDate, sizeof(longDate));
  snprintf(out, outLen, "TIME:%lu|%s|%s|%s|%s",
           (unsigned long)epoch, tod, longDate, stageTimeHealth(), stageTimeSource());
  return true;
}

bool stageTimeSetEpoch(uint32_t epoch) {
  if (!epochInRange(epoch)) return false;
  if (!writeRtc(epoch)) return false;
  sSource = ClockSource::Rtc;
  sSynced = true;
  Serial.printf("[RTC] Operator set %lu\n", (unsigned long)epoch);
  return true;
}

bool stageTimeParseSet(const char *arg, uint32_t *epochOut) {
  if (!arg || !arg[0] || !epochOut) return false;
  while (*arg == ' ') arg++;

  bool allDigits = arg[0] != '\0';
  for (const char *p = arg; *p; p++) {
    if (*p < '0' || *p > '9') {
      allDigits = false;
      break;
    }
  }
  if (allDigits) {
    char *end = nullptr;
    const unsigned long value = strtoul(arg, &end, 10);
    if (!end || end == arg || *end != '\0') return false;
    if (value < SHOWDUINO_RTC_SYNC_FLOOR || value > SHOWDUINO_RTC_MAX_EPOCH) return false;
    *epochOut = (uint32_t)value;
    return true;
  }

  int Y = 0, M = 0, D = 0, h = 0, mi = 0, s = 0;
  if (sscanf(arg, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &mi, &s) < 6 &&
      sscanf(arg, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &mi, &s) < 6) {
    return false;
  }
  if (Y < 2020 || M < 1 || M > 12 || D < 1 || D > 31) return false;
  if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 60) return false;
  const uint32_t epoch = civilToEpoch(Y, M, D, h, mi, s);
  if (!epochInRange(epoch)) return false;
  *epochOut = epoch;
  return true;
}

void stageTimeLoop(uint32_t nowMs, void (*sendFn)(const char *line)) {
  if (!sendFn) return;
  if (sLastPublishMs != 0 && (uint32_t)(nowMs - sLastPublishMs) < SHOWDUINO_RTC_PUBLISH_MS) {
    return;
  }
  sLastPublishMs = nowMs;
  char wire[96];
  if (stageTimeFormatDirectorWire(wire, sizeof(wire))) {
    sendFn(wire);
  }
}
