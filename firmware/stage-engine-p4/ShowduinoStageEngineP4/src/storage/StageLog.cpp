#include "StageLog.h"

#include <string.h>
#include "../../BoardConfig.h"
#include "../StageStorage.h"
#include "../StageTime.h"
#include "StageStore.h"

#if SHOWDUINO_SD_ENABLED

struct LogBuf {
  char data[384];
  size_t len = 0;
};

static LogBuf sBuf[6];
static uint32_t sLastFlushMs = 0;
static uint32_t sUsedBytes = 0;
static bool sFlushPending = false;
static const uint32_t kFlushMs = 5000;

static const char *channelName(StageLogChannel ch) {
  switch (ch) {
    case StageLogChannel::System: return "system";
    case StageLogChannel::Comms: return "comms";
    case StageLogChannel::Network: return "network";
    case StageLogChannel::Emergency: return "emergency";
    case StageLogChannel::Production: return "production";
    case StageLogChannel::Plugin: return "plugin";
    default: return "system";
  }
}

static uint8_t idx(StageLogChannel ch) {
  const uint8_t i = (uint8_t)ch;
  return i < 6 ? i : 0;
}

static void rotateIfNeeded(const char *channel) {
  char path[SHOWDUINO_STORAGE_PATH_MAX + 1];
  char name[32];
  snprintf(name, sizeof(name), "%s.log", channel);
  if (showduino_storage_path_join(PATH_LOGS, name, path, sizeof(path)) != SHOWDUINO_PATH_OK) {
    return;
  }
  fs::FS &fs = stageStorageFs();
  File f = fs.open(path, FILE_READ);
  const size_t sz = f ? f.size() : 0;
  if (f) f.close();
  if (sz < SHOWDUINO_STORAGE_LOG_ROTATE_BYTES) return;

  char oldest[SHOWDUINO_STORAGE_PATH_MAX + 1];
  char rotName[24];
  if (showduino_storage_log_rotated_name(channel, SHOWDUINO_STORAGE_LOG_KEEP, rotName, sizeof(rotName)) &&
      showduino_storage_path_join(PATH_LOGS, rotName, oldest, sizeof(oldest)) == SHOWDUINO_PATH_OK) {
    if (fs.exists(oldest)) fs.remove(oldest);
  }
  for (uint8_t i = SHOWDUINO_STORAGE_LOG_KEEP; i >= 2; i--) {
    char fromName[24], toName[24];
    char fromPath[SHOWDUINO_STORAGE_PATH_MAX + 1];
    char toPath[SHOWDUINO_STORAGE_PATH_MAX + 1];
    if (!showduino_storage_log_rotated_name(channel, (uint8_t)(i - 1), fromName, sizeof(fromName))) continue;
    if (!showduino_storage_log_rotated_name(channel, i, toName, sizeof(toName))) continue;
    if (showduino_storage_path_join(PATH_LOGS, fromName, fromPath, sizeof(fromPath)) != SHOWDUINO_PATH_OK) continue;
    if (showduino_storage_path_join(PATH_LOGS, toName, toPath, sizeof(toPath)) != SHOWDUINO_PATH_OK) continue;
    if (fs.exists(toPath)) fs.remove(toPath);
    if (fs.exists(fromPath)) fs.rename(fromPath, toPath);
  }
  char firstName[24];
  char firstPath[SHOWDUINO_STORAGE_PATH_MAX + 1];
  if (showduino_storage_log_rotated_name(channel, 1, firstName, sizeof(firstName)) &&
      showduino_storage_path_join(PATH_LOGS, firstName, firstPath, sizeof(firstPath)) == SHOWDUINO_PATH_OK) {
    if (fs.exists(firstPath)) fs.remove(firstPath);
    if (fs.exists(path)) fs.rename(path, firstPath);
  }
}

static void flushChannel(uint8_t i) {
  if (sBuf[i].len == 0) return;
  if (!stageStoreWritable()) {
    sBuf[i].len = 0;
    return;
  }
  const char *ch = channelName((StageLogChannel)i);
  rotateIfNeeded(ch);
  char name[32];
  char path[SHOWDUINO_STORAGE_PATH_MAX + 1];
  snprintf(name, sizeof(name), "%s.log", ch);
  if (showduino_storage_path_join(PATH_LOGS, name, path, sizeof(path)) != SHOWDUINO_PATH_OK) {
    sBuf[i].len = 0;
    return;
  }
  File f = stageStorageFs().open(path, FILE_APPEND);
  if (!f) {
    sBuf[i].len = 0;
    return;
  }
  f.write((const uint8_t *)sBuf[i].data, sBuf[i].len);
  f.flush();
  f.close();
  sUsedBytes += (uint32_t)sBuf[i].len;
  sBuf[i].len = 0;
}

static void flushAll() {
  for (uint8_t i = 0; i < 6; i++) flushChannel(i);
  sFlushPending = false;
  sLastFlushMs = millis();
}

void stageLogBegin() {
  memset(sBuf, 0, sizeof(sBuf));
  sUsedBytes = 0;
  sLastFlushMs = millis();
}

void stageLogLoop() {
  if (!sFlushPending) return;
  if ((int32_t)(millis() - sLastFlushMs) < (int32_t)kFlushMs) return;
  flushAll();
}

void stageLogWrite(StageLogChannel channel, const char *level, const char *message) {
  if (!message || !message[0]) return;
  const uint8_t i = idx(channel);
  char stamp[32];
  if (stageTimeSynced()) {
    stageTimeIso(stamp, sizeof(stamp));
  } else {
    snprintf(stamp, sizeof(stamp), "t=%lu", (unsigned long)millis());
  }
  char line[192];
  snprintf(line, sizeof(line), "%s %s %s\n",
           stamp,
           level && level[0] ? level : "INFO",
           message);
  const size_t n = strlen(line);
  if (sBuf[i].len + n >= sizeof(sBuf[i].data)) flushChannel(i);
  if (n >= sizeof(sBuf[i].data)) return;
  memcpy(sBuf[i].data + sBuf[i].len, line, n);
  sBuf[i].len += n;
  sFlushPending = true;
  if (channel == StageLogChannel::Emergency) flushChannel(i);
}

void stageLogEmergency(const char *event, const char *detail) {
  char msg[160];
  snprintf(msg, sizeof(msg), "%s %s",
           event ? event : "EVENT",
           detail ? detail : "");
  stageLogWrite(StageLogChannel::Emergency, "EMERGENCY", msg);
}

uint32_t stageLogUsedBytes() {
  return sUsedBytes;
}

#else

void stageLogBegin() {}
void stageLogLoop() {}
void stageLogWrite(StageLogChannel, const char *, const char *) {}
void stageLogEmergency(const char *, const char *) {}
uint32_t stageLogUsedBytes() { return 0; }

#endif
