#include "ShowduinoIO.h"

#include "../BoardConfig.h"
#include "StageStorage.h"
#include "storage/StageStore.h"

#include <FS.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t kLineCount = 2;
constexpr uint32_t kMaxPulseMs = 3600000UL;
constexpr uint16_t kMaxDebounceMs = 5000U;
constexpr size_t kConfigMaxBytes = 2048;

struct IOChannel {
  uint8_t line = 0;
  int pin = -1;
  ShowduinoIOMode mode = ShowduinoIOMode::Disabled;
  ShowduinoIOPull pull = ShowduinoIOPull::None;
  bool activeHigh = true;
  uint16_t debounceMs = SHOWDUINO_GENERIC_IO_DEFAULT_DEBOUNCE_MS;

  int rawLevel = LOW;
  int stableLevel = LOW;
  uint32_t rawChangedMs = 0;

  bool outputActive = false;
  bool pulseActive = false;
  uint32_t pulseUntilMs = 0;
};

IOChannel sLines[kLineCount];
ShowduinoIOSendFn sSend = nullptr;
bool sEmergency = false;
bool sBegun = false;

const char *modeName(ShowduinoIOMode mode) {
  switch (mode) {
    case ShowduinoIOMode::Input: return "INPUT";
    case ShowduinoIOMode::Output: return "OUTPUT";
    default: return "DISABLED";
  }
}

const char *pullName(ShowduinoIOPull pull) {
  switch (pull) {
    case ShowduinoIOPull::Up: return "UP";
    case ShowduinoIOPull::Down: return "DOWN";
    default: return "NONE";
  }
}

int activeLevel(const IOChannel &ch) {
  return ch.activeHigh ? HIGH : LOW;
}

int inactiveLevel(const IOChannel &ch) {
  return ch.activeHigh ? LOW : HIGH;
}

bool logicalActive(const IOChannel &ch, int level) {
  return level == activeLevel(ch);
}

IOChannel *lineByNumber(unsigned line) {
  if (line < 1 || line > kLineCount) return nullptr;
  return &sLines[line - 1];
}

void resetDefaults() {
  sLines[0] = IOChannel{};
  sLines[0].line = 1;
  sLines[0].pin = SHOWDUINO_GENERIC_IO_1_PIN;

  sLines[1] = IOChannel{};
  sLines[1].line = 2;
  sLines[1].pin = SHOWDUINO_GENERIC_IO_2_PIN;
}

void makeDisabled(IOChannel &ch) {
  ch.outputActive = false;
  ch.pulseActive = false;
  ch.pulseUntilMs = 0;
  pinMode(ch.pin, INPUT);
  ch.rawLevel = digitalRead(ch.pin);
  ch.stableLevel = ch.rawLevel;
  ch.rawChangedMs = millis();
}

void makeInput(IOChannel &ch) {
  ch.outputActive = false;
  ch.pulseActive = false;
  ch.pulseUntilMs = 0;

  if (ch.pull == ShowduinoIOPull::Up) {
    pinMode(ch.pin, INPUT_PULLUP);
  } else if (ch.pull == ShowduinoIOPull::Down) {
    pinMode(ch.pin, INPUT_PULLDOWN);
  } else {
    pinMode(ch.pin, INPUT);
  }

  ch.rawLevel = digitalRead(ch.pin);
  ch.stableLevel = ch.rawLevel;
  ch.rawChangedMs = millis();
}

void writeOutput(IOChannel &ch, bool active) {
  if (active && sEmergency) active = false;
  ch.outputActive = active;
  ch.pulseActive = false;
  ch.pulseUntilMs = 0;
  digitalWrite(ch.pin, active ? activeLevel(ch) : inactiveLevel(ch));
}

void makeOutput(IOChannel &ch) {
  /*
   * Pre-load the inactive output latch before switching the pad to OUTPUT.
   * This avoids a brief active pulse when ACTIVE=LOW.
   */
  ch.outputActive = false;
  ch.pulseActive = false;
  ch.pulseUntilMs = 0;
  digitalWrite(ch.pin, inactiveLevel(ch));
  pinMode(ch.pin, OUTPUT);
  digitalWrite(ch.pin, inactiveLevel(ch));
  ch.rawLevel = inactiveLevel(ch);
  ch.stableLevel = ch.rawLevel;
  ch.rawChangedMs = millis();
}

void applyMode(IOChannel &ch) {
  switch (ch.mode) {
    case ShowduinoIOMode::Input:
      makeInput(ch);
      break;
    case ShowduinoIOMode::Output:
      makeOutput(ch);
      break;
    default:
      makeDisabled(ch);
      break;
  }
}

void emitLineState(const IOChannel &ch) {
  if (!sSend) return;

  char line[128];
  if (ch.mode == ShowduinoIOMode::Disabled) {
    snprintf(line, sizeof(line),
             "STATE:IO:%u:MODE:DISABLED:GPIO:%d",
             (unsigned)ch.line, ch.pin);
  } else {
    const int level = (ch.mode == ShowduinoIOMode::Input)
                        ? ch.stableLevel
                        : (ch.outputActive ? activeLevel(ch) : inactiveLevel(ch));
    snprintf(line, sizeof(line),
             "STATE:IO:%u:MODE:%s:GPIO:%d:LEVEL:%s:ACTIVE:%u",
             (unsigned)ch.line,
             modeName(ch.mode),
             ch.pin,
             level == HIGH ? "HIGH" : "LOW",
             logicalActive(ch, level) ? 1U : 0U);
  }
  sSend(line);
  snprintf(line,sizeof(line),"STATE:IO:%u:CFG:%u,%u,%u,%u",ch.line,(unsigned)ch.mode,ch.activeHigh?1U:0U,(unsigned)ch.pull,ch.debounceMs);
  sSend(line);
}

void emitInputEdge(const IOChannel &ch) {
  Serial.printf("[IO] line=%u GPIO%d input %s (%s)\n",
                (unsigned)ch.line,
                ch.pin,
                ch.stableLevel == HIGH ? "HIGH" : "LOW",
                logicalActive(ch, ch.stableLevel) ? "ACTIVE" : "INACTIVE");
  emitLineState(ch);
}

bool jsonString(const String &json, const char *key, char *out, size_t outLen) {
  if (!key || !out || outLen == 0) return false;
  String needle = String("\"") + key + "\"";
  int pos = json.indexOf(needle);
  if (pos < 0) return false;
  int colon = json.indexOf(':', pos + needle.length());
  if (colon < 0) return false;
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return false;
  int q2 = json.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  String value = json.substring(q1 + 1, q2);
  value.trim();
  strncpy(out, value.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
  return true;
}

bool jsonUInt(const String &json, const char *key, uint32_t *out) {
  if (!key || !out) return false;
  String needle = String("\"") + key + "\"";
  int pos = json.indexOf(needle);
  if (pos < 0) return false;
  int colon = json.indexOf(':', pos + needle.length());
  if (colon < 0) return false;
  const char *start = json.c_str() + colon + 1;
  while (*start && isspace((unsigned char)*start)) start++;
  if (!isdigit((unsigned char)*start)) return false;
  char *end = nullptr;
  unsigned long value = strtoul(start, &end, 10);
  if (end == start) return false;
  *out = (uint32_t)value;
  return true;
}

bool lineObject(const String &json, unsigned wantedLine, String &out) {
  int searchFrom = 0;
  while (true) {
    int key = json.indexOf("\"line\"", searchFrom);
    if (key < 0) return false;

    int colon = json.indexOf(':', key + 6);
    if (colon < 0) return false;
    const char *start = json.c_str() + colon + 1;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = nullptr;
    unsigned long line = strtoul(start, &end, 10);

    if (end != start && line == wantedLine) {
      int open = key;
      while (open >= 0 && json.charAt(open) != '{') open--;
      if (open < 0) return false;

      int depth = 0;
      for (int i = open; i < (int)json.length(); ++i) {
        const char c = json.charAt(i);
        if (c == '{') depth++;
        else if (c == '}') {
          depth--;
          if (depth == 0) {
            out = json.substring(open, i + 1);
            return true;
          }
        }
      }
      return false;
    }

    searchFrom = key + 6;
  }
}

bool parseMode(const char *value, ShowduinoIOMode *out) {
  if (!value || !out) return false;
  if (strcmp(value, "DISABLED") == 0) {
    *out = ShowduinoIOMode::Disabled;
    return true;
  }
  if (strcmp(value, "INPUT") == 0) {
    *out = ShowduinoIOMode::Input;
    return true;
  }
  if (strcmp(value, "OUTPUT") == 0) {
    *out = ShowduinoIOMode::Output;
    return true;
  }
  return false;
}

bool parsePull(const char *value, ShowduinoIOPull *out) {
  if (!value || !out) return false;
  if (strcmp(value, "NONE") == 0) {
    *out = ShowduinoIOPull::None;
    return true;
  }
  if (strcmp(value, "UP") == 0) {
    *out = ShowduinoIOPull::Up;
    return true;
  }
  if (strcmp(value, "DOWN") == 0) {
    *out = ShowduinoIOPull::Down;
    return true;
  }
  return false;
}

bool parseLineConfig(const String &object, IOChannel &ch) {
  char mode[12] = "";
  char active[8] = "";
  char pull[8] = "";
  uint32_t debounce = SHOWDUINO_GENERIC_IO_DEFAULT_DEBOUNCE_MS;

  if (!jsonString(object, "mode", mode, sizeof(mode)) ||
      !jsonString(object, "active", active, sizeof(active)) ||
      !jsonString(object, "pull", pull, sizeof(pull)) ||
      !jsonUInt(object, "debounce_ms", &debounce)) {
    return false;
  }

  ShowduinoIOMode parsedMode;
  ShowduinoIOPull parsedPull;
  if (!parseMode(mode, &parsedMode) || !parsePull(pull, &parsedPull)) return false;
  if (strcmp(active, "HIGH") != 0 && strcmp(active, "LOW") != 0) return false;
  if (debounce > kMaxDebounceMs) return false;

  ch.mode = parsedMode;
  ch.pull = parsedPull;
  ch.activeHigh = strcmp(active, "HIGH") == 0;
  ch.debounceMs = (uint16_t)debounce;
  return true;
}

String configJson() {
  String out;
  out.reserve(620);
  out += "{\n";
  out += "  \"formatVersion\": 1,\n";
  out += "  \"lines\": [\n";

  for (uint8_t i = 0; i < kLineCount; ++i) {
    const IOChannel &ch = sLines[i];
    out += "    {\"line\": ";
    out += ch.line;
    out += ", \"gpio\": ";
    out += ch.pin;
    out += ", \"mode\": \"";
    out += modeName(ch.mode);
    out += "\", \"active\": \"";
    out += ch.activeHigh ? "HIGH" : "LOW";
    out += "\", \"pull\": \"";
    out += pullName(ch.pull);
    out += "\", \"debounce_ms\": ";
    out += ch.debounceMs;
    out += ", \"bootState\": \"INACTIVE\", \"failsafeState\": \"INACTIVE\"}";
    out += (i + 1 < kLineCount) ? ",\n" : "\n";
  }

  out += "  ]\n";
  out += "}\n";
  return out;
}

bool saveConfig() {
  if (!stageStoreWritable()) return false;
  const String json = configJson();
  const bool ok = stageStoreAtomicWrite(PATH_GENERIC_IO_CONFIG, json.c_str(), json.length());
  if (ok) {
    Serial.println("[IO] Saved /showduino/config/io.json");
  } else {
    Serial.println("[IO] WARN: io.json atomic write failed");
  }
  return ok;
}

bool loadConfigFromSd() {
  if (!stageStorageIsReady()) return false;
  if (!stageStorageFs().exists(PATH_GENERIC_IO_CONFIG)) return false;

  File file = stageStorageFs().open(PATH_GENERIC_IO_CONFIG, FILE_READ);
  if (!file) {
    Serial.println("[IO] WARN: io.json cannot be opened");
    return false;
  }
  if (file.size() > kConfigMaxBytes) {
    Serial.println("[IO] WARN: io.json too large — defaults retained");
    file.close();
    return false;
  }

  String json = file.readString();
  file.close();

  uint32_t version = 0;
  if (!jsonUInt(json, "formatVersion", &version) || version != 1) {
    Serial.println("[IO] WARN: io.json formatVersion invalid — defaults retained");
    return false;
  }

  IOChannel parsed[kLineCount] = {sLines[0], sLines[1]};
  for (unsigned line = 1; line <= kLineCount; ++line) {
    String object;
    if (!lineObject(json, line, object) ||
        !parseLineConfig(object, parsed[line - 1])) {
      Serial.printf("[IO] WARN: io.json line %u invalid — defaults retained\n", line);
      return false;
    }
  }

  for (uint8_t i = 0; i < kLineCount; ++i) {
    sLines[i].mode = parsed[i].mode;
    sLines[i].pull = parsed[i].pull;
    sLines[i].activeHigh = parsed[i].activeHigh;
    sLines[i].debounceMs = parsed[i].debounceMs;
    applyMode(sLines[i]);
  }

  Serial.println("[IO] Loaded /showduino/config/io.json");
  return true;
}

void formatChannelStatus(const IOChannel &ch, char *reply, size_t replyLen) {
  if (!reply || replyLen == 0) return;

  if (ch.mode == ShowduinoIOMode::Disabled) {
    snprintf(reply, replyLen,
             "IO:%u:STATUS:MODE=DISABLED:GPIO=%d:ACTIVE_LEVEL=%s:PULL=%s:DEBOUNCE=%u",
             (unsigned)ch.line,
             ch.pin,
             ch.activeHigh ? "HIGH" : "LOW",
             pullName(ch.pull),
             (unsigned)ch.debounceMs);
    return;
  }

  const int level = (ch.mode == ShowduinoIOMode::Input)
                      ? ch.stableLevel
                      : (ch.outputActive ? activeLevel(ch) : inactiveLevel(ch));

  snprintf(reply, replyLen,
           "IO:%u:STATUS:MODE=%s:GPIO=%d:LEVEL=%s:ACTIVE=%u:ACTIVE_LEVEL=%s:PULL=%s:DEBOUNCE=%u",
           (unsigned)ch.line,
           modeName(ch.mode),
           ch.pin,
           level == HIGH ? "HIGH" : "LOW",
           logicalActive(ch, level) ? 1U : 0U,
           ch.activeHigh ? "HIGH" : "LOW",
           pullName(ch.pull),
           (unsigned)ch.debounceMs);
}

void configReply(char *reply, size_t replyLen, unsigned line,
                 const char *field, const char *value, bool saved) {
  snprintf(reply, replyLen, "IO:%u:%s:%s:OK%s",
           line,
           field,
           value,
           saved ? "" : ":RAM_ONLY");
}

bool outputRequired(IOChannel &ch, char *reply, size_t replyLen) {
  if (ch.mode == ShowduinoIOMode::Output) return true;
  snprintf(reply, replyLen, "IO:%u:ERROR:NOT_OUTPUT", (unsigned)ch.line);
  return false;
}

} // namespace

void showduinoIOBegin(ShowduinoIOSendFn sendFn) {
  sSend = sendFn;
  sEmergency = false;
  resetDefaults();

  /*
   * Explicitly make both pads high impedance before any persistent config is
   * considered. This is the safe state during early boot and SD failure.
   */
  for (uint8_t i = 0; i < kLineCount; ++i) {
    makeDisabled(sLines[i]);
  }

  sBegun = true;
  Serial.printf("[IO] Generic I/O safe boot: line1=GPIO%d line2=GPIO%d DISABLED\n",
                SHOWDUINO_GENERIC_IO_1_PIN,
                SHOWDUINO_GENERIC_IO_2_PIN);

  if (stageStorageIsReady()) showduinoIOReloadConfig();
}

void showduinoIOReloadConfig() {
  if (!sBegun) return;

  /*
   * Always reset to safe defaults before parsing. A missing or corrupt file
   * can therefore never leave stale output settings behind.
   */
  resetDefaults();
  for (uint8_t i = 0; i < kLineCount; ++i) makeDisabled(sLines[i]);

  if (!stageStorageIsReady()) {
    Serial.println("[IO] No SD — Generic I/O remains DISABLED");
    return;
  }

  if (!stageStorageFs().exists(PATH_GENERIC_IO_CONFIG)) {
    Serial.println("[IO] io.json missing — writing safe defaults");
    if (!saveConfig()) {
      Serial.println("[IO] WARN: defaults are RAM-only");
    }
    return;
  }

  if (!loadConfigFromSd()) {
    Serial.println("[IO] Generic I/O remains safely DISABLED");
  }
}

void showduinoIOService() {
  if (!sBegun) return;
  const uint32_t now = millis();

  for (uint8_t i = 0; i < kLineCount; ++i) {
    IOChannel &ch = sLines[i];

    if (ch.mode == ShowduinoIOMode::Input) {
      const int raw = digitalRead(ch.pin);
      if (raw != ch.rawLevel) {
        ch.rawLevel = raw;
        ch.rawChangedMs = now;
      }

      if (ch.rawLevel != ch.stableLevel &&
          (uint32_t)(now - ch.rawChangedMs) >= ch.debounceMs) {
        ch.stableLevel = ch.rawLevel;
        emitInputEdge(ch);
      }
      continue;
    }

    if (ch.mode == ShowduinoIOMode::Output &&
        ch.pulseActive &&
        (int32_t)(now - ch.pulseUntilMs) >= 0) {
      ch.pulseActive = false;
      ch.pulseUntilMs = 0;
      ch.outputActive = false;
      digitalWrite(ch.pin, inactiveLevel(ch));
      Serial.printf("[IO] line=%u pulse complete -> INACTIVE\n", (unsigned)ch.line);
      emitLineState(ch);
    }
  }
}

void showduinoIOOnEmergency(bool active) {
  sEmergency = active;
  if (active) {
    showduinoIOAllOff("EMERGENCY");
    Serial.println("[IO] Emergency override — all generic outputs INACTIVE");
  } else {
    /*
     * Do not restore previous states. Outputs remain inactive until an explicit
     * post-clear command or a later show cue drives them again.
     */
    showduinoIOAllOff("EMERGENCY_CLEAR");
    Serial.println("[IO] Emergency cleared — generic outputs remain INACTIVE");
  }
}

void showduinoIOAllOff(const char *reason) {
  for (uint8_t i = 0; i < kLineCount; ++i) {
    IOChannel &ch = sLines[i];
    if (ch.mode != ShowduinoIOMode::Output) continue;
    ch.pulseActive = false;
    ch.pulseUntilMs = 0;
    ch.outputActive = false;
    digitalWrite(ch.pin, inactiveLevel(ch));
  }
  if (reason && reason[0]) {
    Serial.printf("[IO] all outputs INACTIVE reason=%s\n", reason);
  }
}

void showduinoIOPublishState() {
  if (!sBegun) return;
  for (uint8_t i = 0; i < kLineCount; ++i) emitLineState(sLines[i]);
}

bool showduinoIOHandleCommand(const char *command, char *reply, size_t replyLen) {
  if (!command || !reply || replyLen == 0) return false;
  reply[0] = '\0';

  String cmd(command);
  cmd.trim();
  cmd.toUpperCase();
  if (!cmd.startsWith("IO:")) return false;

  if (cmd == "IO:STATUS") {
    showduinoIOPublishState();
    snprintf(reply, replyLen,
             "IO:STATUS:1=%s@GPIO%d:2=%s@GPIO%d:EMERGENCY=%u",
             modeName(sLines[0].mode), sLines[0].pin,
             modeName(sLines[1].mode), sLines[1].pin,
             sEmergency ? 1U : 0U);
    return true;
  }

  if (cmd == "IO:SAVE") {
    snprintf(reply, replyLen, saveConfig() ? "IO:SAVE:OK" : "IO:SAVE:ERROR:NOT_WRITABLE");
    return true;
  }

  if (cmd == "IO:ALL:OFF") {
    showduinoIOAllOff("COMMAND");
    snprintf(reply, replyLen, "IO:ALL:OFF:OK");
    return true;
  }

  const int separator = cmd.indexOf(':', 3);
  if (separator < 0) {
    snprintf(reply, replyLen, "IO:ERROR:BAD_COMMAND");
    return true;
  }

  const unsigned line = (unsigned)cmd.substring(3, separator).toInt();
  IOChannel *ch = lineByNumber(line);
  if (!ch) {
    snprintf(reply, replyLen, "IO:ERROR:BAD_LINE");
    return true;
  }

  String action = cmd.substring(separator + 1);

  if (action == "STATUS" || action == "GET") {
    formatChannelStatus(*ch, reply, replyLen);
    return true;
  }

  if (action.startsWith("CONFIG:")) {
    char mode[9]={},high[5]={},pull[5]={};unsigned debounce=0;int end=0;
    ShowduinoIOMode parsedMode;ShowduinoIOPull parsedPull;
    if(sEmergency || sscanf(action.c_str(),"CONFIG:%8[^:]:%4[^:]:%4[^:]:%u%n",mode,high,pull,&debounce,&end)!=4 || action.c_str()[end] || !parseMode(mode,&parsedMode) || !parsePull(pull,&parsedPull) || (strcmp(high,"HIGH")&&strcmp(high,"LOW")) || debounce>kMaxDebounceMs) {
      snprintf(reply,replyLen,"IO:%u:CONFIG:ERROR:INVALID_OR_EMERGENCY",line);return true;
    }
    writeOutput(*ch,false);makeDisabled(*ch);
    ch->mode=parsedMode;ch->activeHigh=!strcmp(high,"HIGH");ch->pull=parsedPull;ch->debounceMs=debounce;applyMode(*ch);
    const bool saved=saveConfig();
    snprintf(reply,replyLen,"IO:%u:CONFIG:%s",line,saved?"SAVED":"RAM_ONLY_SD_ERROR");emitLineState(*ch);return true;
  }
  if (action.startsWith("MODE:")) {
    const String value = action.substring(5);
    ShowduinoIOMode mode;
    if (!parseMode(value.c_str(), &mode)) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_MODE", line);
      return true;
    }
    ch->mode = mode;
    applyMode(*ch);
    configReply(reply, replyLen, line, "MODE", modeName(mode), saveConfig());
    emitLineState(*ch);
    return true;
  }

  if (action.startsWith("ACTIVE:")) {
    const String value = action.substring(7);
    if (value != "HIGH" && value != "LOW") {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_ACTIVE_LEVEL", line);
      return true;
    }
    ch->activeHigh = value == "HIGH";
    if (ch->mode == ShowduinoIOMode::Output) makeOutput(*ch);
    configReply(reply, replyLen, line, "ACTIVE", ch->activeHigh ? "HIGH" : "LOW", saveConfig());
    emitLineState(*ch);
    return true;
  }

  if (action.startsWith("PULL:")) {
    const String value = action.substring(5);
    ShowduinoIOPull pull;
    if (!parsePull(value.c_str(), &pull)) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_PULL", line);
      return true;
    }
    ch->pull = pull;
    if (ch->mode == ShowduinoIOMode::Input) makeInput(*ch);
    configReply(reply, replyLen, line, "PULL", pullName(pull), saveConfig());
    emitLineState(*ch);
    return true;
  }

  if (action.startsWith("DEBOUNCE:")) {
    const String value = action.substring(9);
    if (value.length() == 0) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_DEBOUNCE", line);
      return true;
    }
    for (unsigned i = 0; i < value.length(); ++i) {
      if (!isdigit((unsigned char)value.charAt(i))) {
        snprintf(reply, replyLen, "IO:%u:ERROR:BAD_DEBOUNCE", line);
        return true;
      }
    }
    const unsigned long debounce = strtoul(value.c_str(), nullptr, 10);
    if (debounce > kMaxDebounceMs) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_DEBOUNCE", line);
      return true;
    }
    ch->debounceMs = (uint16_t)debounce;
    char valueBuf[12];
    snprintf(valueBuf, sizeof(valueBuf), "%u", (unsigned)ch->debounceMs);
    configReply(reply, replyLen, line, "DEBOUNCE", valueBuf, saveConfig());
    return true;
  }

  if (action == "OFF") {
    if (!outputRequired(*ch, reply, replyLen)) return true;
    writeOutput(*ch, false);
    snprintf(reply, replyLen, "IO:%u:OFF:OK", line);
    emitLineState(*ch);
    return true;
  }

  if (action == "ON") {
    if (!outputRequired(*ch, reply, replyLen)) return true;
    if (sEmergency) {
      snprintf(reply, replyLen, "IO:%u:ON:REJECTED:EMERGENCY", line);
      return true;
    }
    writeOutput(*ch, true);
    snprintf(reply, replyLen, "IO:%u:ON:OK", line);
    emitLineState(*ch);
    return true;
  }

  if (action == "TOGGLE") {
    if (!outputRequired(*ch, reply, replyLen)) return true;
    if (sEmergency) {
      snprintf(reply, replyLen, "IO:%u:TOGGLE:REJECTED:EMERGENCY", line);
      return true;
    }
    writeOutput(*ch, !ch->outputActive);
    snprintf(reply, replyLen, "IO:%u:TOGGLE:OK", line);
    emitLineState(*ch);
    return true;
  }

  if (action.startsWith("PULSE:")) {
    if (!outputRequired(*ch, reply, replyLen)) return true;
    if (sEmergency) {
      snprintf(reply, replyLen, "IO:%u:PULSE:REJECTED:EMERGENCY", line);
      return true;
    }

    const String value = action.substring(6);
    if (value.length() == 0) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_PULSE", line);
      return true;
    }
    for (unsigned i = 0; i < value.length(); ++i) {
      if (!isdigit((unsigned char)value.charAt(i))) {
        snprintf(reply, replyLen, "IO:%u:ERROR:BAD_PULSE", line);
        return true;
      }
    }

    const unsigned long pulseMs = strtoul(value.c_str(), nullptr, 10);
    if (pulseMs < 1 || pulseMs > kMaxPulseMs) {
      snprintf(reply, replyLen, "IO:%u:ERROR:BAD_PULSE", line);
      return true;
    }

    ch->outputActive = true;
    ch->pulseActive = true;
    ch->pulseUntilMs = millis() + (uint32_t)pulseMs;
    digitalWrite(ch->pin, activeLevel(*ch));
    snprintf(reply, replyLen, "IO:%u:PULSE:%lu:OK", line, pulseMs);
    emitLineState(*ch);
    return true;
  }

  snprintf(reply, replyLen, "IO:%u:ERROR:BAD_COMMAND", line);
  return true;
}
