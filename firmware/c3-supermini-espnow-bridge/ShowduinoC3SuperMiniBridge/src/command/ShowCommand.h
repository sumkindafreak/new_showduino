#ifndef SHOWDUINO_SHOW_COMMAND_H
#define SHOWDUINO_SHOW_COMMAND_H

#include <Arduino.h>
#include <stdint.h>

enum class CommandCategory : uint8_t {
  System = 0,
  Scene,
  Show,
  Lighting,
  Audio,
  Relay,
  Gpio,
  Dmx,
  Media,
  Network,
  Emergency,
  Custom = 255
};

enum class CommandPriority : uint8_t {
  Normal = 0,
  High = 1,
  Emergency = 2
};

enum class CommandStatus : uint8_t {
  Received = 0,
  Validated,
  Rejected,
  Queued,
  Started,
  Completed,
  Cancelled,
  Failed
};

struct ShowCommand {
  char id[20] = {0};
  uint32_t timestampMs = 0;
  uint32_t createdEpoch = 0;
  uint32_t queuedEpoch = 0;
  uint32_t startedEpoch = 0;
  uint32_t completedEpoch = 0;
  char source[28] = {0};
  char destination[28] = {0};
  CommandCategory category = CommandCategory::System;
  char action[36] = {0};
  char payload[160] = {0};
  CommandPriority priority = CommandPriority::Normal;
  CommandStatus status = CommandStatus::Received;
  char result[72] = {0};
  uint32_t executionTimeMs = 0;
  uint32_t startedMs = 0;
  uint32_t completedMs = 0;
  bool inUse = false;
};

const char *commandCategoryName(CommandCategory c);
bool commandCategoryFromName(const char *name, CommandCategory &out);
const char *commandPriorityName(CommandPriority p);
bool commandPriorityFromName(const char *name, CommandPriority &out);
const char *commandStatusName(CommandStatus s);
bool commandStatusFromName(const char *name, CommandStatus &out);

bool showCommandKnownSource(const char *source);
bool showCommandKnownDestination(const char *destination);

void showCommandAssignId(ShowCommand &cmd, uint32_t seq);
void showCommandToJson(const ShowCommand &cmd, String &out);
bool showCommandFromJson(const String &json, ShowCommand &out, String &error);

#endif