#ifndef SHOWDUINO_PRODUCTION_FORMAT_H
#define SHOWDUINO_PRODUCTION_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define SHOWDUINO_PRODUCTION_FORMAT_VERSION 1U
#define SHOWDUINO_TIMELINE_FORMAT_VERSION   1U
#define SHOWDUINO_PRODUCTION_ID_MAX         48U
#define SHOWDUINO_PRODUCTION_NAME_MAX       64U
#define SHOWDUINO_PRODUCTION_TEXT_MAX       128U
#define SHOWDUINO_PRODUCTION_AUTHOR_MAX     64U
#define SHOWDUINO_PRODUCTION_PATH_MAX       80U
#define SHOWDUINO_CUE_ID_MAX                40U
#define SHOWDUINO_CUE_TARGET_MAX            64U
#define SHOWDUINO_CUE_ACTION_MAX            16U
#define SHOWDUINO_CUE_VALUE_MAX             96U
#define SHOWDUINO_CUE_COMMAND_MAX           64U
#define SHOWDUINO_PRODUCTION_MAX_CUES       512U

enum class ProductionParseResult : uint8_t {
  Ok = 0,
  InvalidJson,
  MissingField,
  UnsupportedVersion,
  InvalidProductionId,
  InvalidTimelinePath,
  EmptyTimeline,
  TooManyCues,
  DuplicateCueId,
  InvalidCueTime,
  UnsupportedCueType,
  InvalidCueAction,
  CommandTooLong
};

struct ProductionManifest {
  uint16_t formatVersion = 0;
  char productionId[SHOWDUINO_PRODUCTION_ID_MAX] = {};
  char name[SHOWDUINO_PRODUCTION_NAME_MAX] = {};
  char description[SHOWDUINO_PRODUCTION_TEXT_MAX] = {};
  char author[SHOWDUINO_PRODUCTION_AUTHOR_MAX] = {};
  char timeline[SHOWDUINO_PRODUCTION_PATH_MAX] = {};
  char created[24] = {};
  char modified[24] = {};
  uint32_t revision = 0;
};

struct ProductionCue {
  char id[SHOWDUINO_CUE_ID_MAX] = {};
  uint32_t timeMs = 0;
  char type[8] = {};
  char target[SHOWDUINO_CUE_TARGET_MAX] = {};
  char action[SHOWDUINO_CUE_ACTION_MAX] = {};
  char value[SHOWDUINO_CUE_VALUE_MAX] = {};
  char command[SHOWDUINO_CUE_COMMAND_MAX] = {};
};

struct ProductionTimeline {
  uint16_t formatVersion = 0;
  uint16_t cueCount = 0;
};

bool productionIdIsValid(const char *id);
bool productionRelativePathIsValid(const char *path);
const char *productionParseResultName(ProductionParseResult result);

bool productionParseManifest(const char *json, size_t jsonLen,
                             ProductionManifest *out,
                             ProductionParseResult *result);

bool productionParseTimeline(const char *json, size_t jsonLen,
                             ProductionCue *cueBuffer, size_t cueCapacity,
                             ProductionTimeline *out,
                             ProductionParseResult *result);

#endif
