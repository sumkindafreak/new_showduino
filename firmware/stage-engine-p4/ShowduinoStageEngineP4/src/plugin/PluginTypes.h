#ifndef SHOWDUINO_PLUGIN_TYPES_H
#define SHOWDUINO_PLUGIN_TYPES_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#define PLUGIN_MAX_INSTANCES     20
#define PLUGIN_MAX_DEFS          16
#define PLUGIN_MAX_JSON_BYTES    3072
#define PLUGIN_ID_LEN            28
#define PLUGIN_NAME_LEN          32
#define PLUGIN_DRIVER_LEN        24
#define PLUGIN_SCHEMA_VERSION    1

#define PLUGIN_ADDR_MIN          0x08
#define PLUGIN_ADDR_MAX          0x77
#define PLUGIN_MUX_NONE          0x00
#define PLUGIN_MUX_CH_NONE       0xFF

enum class PluginStatus : uint8_t {
  Absent = 0,
  Online,
  Offline,
  Unknown,
  Ambiguous
};

enum class PluginSafeState : uint8_t {
  Off = 0,
  Hold,
  Custom
};

enum PluginCap : uint32_t {
  PLUGIN_CAP_NONE            = 0,
  PLUGIN_CAP_DIGITAL_IN      = 1u << 0,
  PLUGIN_CAP_DIGITAL_OUT     = 1u << 1,
  PLUGIN_CAP_ANALOG_IN       = 1u << 2,
  PLUGIN_CAP_ANALOG_OUT      = 1u << 3,
  PLUGIN_CAP_PWM_OUT         = 1u << 4,
  PLUGIN_CAP_SERVO_OUT       = 1u << 5,
  PLUGIN_CAP_TEMPERATURE     = 1u << 6,
  PLUGIN_CAP_HUMIDITY        = 1u << 7,
  PLUGIN_CAP_PRESSURE        = 1u << 8,
  PLUGIN_CAP_DISTANCE        = 1u << 9,
  PLUGIN_CAP_MOTION          = 1u << 10,
  PLUGIN_CAP_ORIENTATION     = 1u << 11,
  PLUGIN_CAP_ACCEL           = 1u << 12,
  PLUGIN_CAP_LIGHT           = 1u << 13,
  PLUGIN_CAP_COLOUR          = 1u << 14,
  PLUGIN_CAP_CURRENT         = 1u << 15,
  PLUGIN_CAP_VOLTAGE         = 1u << 16,
  PLUGIN_CAP_POWER           = 1u << 17,
  PLUGIN_CAP_CLOCK           = 1u << 18,
  PLUGIN_CAP_DISPLAY         = 1u << 19,
  PLUGIN_CAP_STORAGE         = 1u << 20,
  PLUGIN_CAP_BUTTON          = 1u << 21,
  PLUGIN_CAP_ENCODER         = 1u << 22,
  PLUGIN_CAP_MUX             = 1u << 23
};

struct PluginLocation {
  uint8_t busId = 0;
  uint8_t address = 0;
  uint8_t muxAddr = PLUGIN_MUX_NONE;
  uint8_t muxChannel = PLUGIN_MUX_CH_NONE;
};

struct PluginIdentify {
  bool enabled = false;
  uint8_t reg = 0;
  uint8_t mask = 0xFF;
  uint8_t equals = 0;
};

struct PluginDef {
  char id[PLUGIN_ID_LEN] = {};
  char name[PLUGIN_NAME_LEN] = {};
  char driver[PLUGIN_DRIVER_LEN] = {};
  uint8_t addrMin = 0;
  uint8_t addrMax = 0;
  uint32_t capabilities = 0;
  PluginIdentify identify;
  PluginSafeState safeState = PluginSafeState::Off;
  bool fromSd = false;
};

struct PluginConfigInstance {
  char instanceId[PLUGIN_ID_LEN] = {};
  char deviceId[PLUGIN_ID_LEN] = {};
  char friendly[PLUGIN_NAME_LEN] = {};
  PluginLocation loc;
  bool used = false;
};

struct PluginInstance {
  PluginLocation loc;
  char instanceId[PLUGIN_ID_LEN] = {};
  char deviceId[PLUGIN_ID_LEN] = {};
  char friendly[PLUGIN_NAME_LEN] = {};
  char driver[PLUGIN_DRIVER_LEN] = {};
  uint32_t capabilities = 0;
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
  PluginStatus status = PluginStatus::Absent;
  PluginSafeState safeState = PluginSafeState::Off;
  bool configured = false;
  bool identityFromAddressOnly = false;
};

struct PluginBusSelfTest {
  bool busInit = false;
  bool sdaIdleHigh = false;
  bool sclIdleHigh = false;
  bool scanOk = false;
  bool definitionsOk = true;
  uint8_t devicesFound = 0;
  uint8_t known = 0;
  uint8_t unknown = 0;
  uint8_t offlineConfigured = 0;
  char detail[48] = {};
};

inline bool pluginLocationEqual(const PluginLocation &a, const PluginLocation &b) {
  return a.busId == b.busId && a.address == b.address &&
         a.muxAddr == b.muxAddr && a.muxChannel == b.muxChannel;
}

inline const char *pluginStatusName(PluginStatus s) {
  switch (s) {
    case PluginStatus::Online: return "ONLINE";
    case PluginStatus::Offline: return "OFFLINE";
    case PluginStatus::Unknown: return "UNKNOWN";
    case PluginStatus::Ambiguous: return "AMBIGUOUS";
    case PluginStatus::Absent:
    default: return "ABSENT";
  }
}

inline const char *pluginCapName(PluginCap cap) {
  switch (cap) {
    case PLUGIN_CAP_DIGITAL_IN: return "digital.input";
    case PLUGIN_CAP_DIGITAL_OUT: return "digital.output";
    case PLUGIN_CAP_ANALOG_IN: return "analog.input";
    case PLUGIN_CAP_ANALOG_OUT: return "analog.output";
    case PLUGIN_CAP_PWM_OUT: return "pwm.output";
    case PLUGIN_CAP_SERVO_OUT: return "servo.output";
    case PLUGIN_CAP_TEMPERATURE: return "temperature";
    case PLUGIN_CAP_HUMIDITY: return "humidity";
    case PLUGIN_CAP_PRESSURE: return "pressure";
    case PLUGIN_CAP_DISTANCE: return "distance";
    case PLUGIN_CAP_MOTION: return "motion";
    case PLUGIN_CAP_ORIENTATION: return "orientation";
    case PLUGIN_CAP_ACCEL: return "acceleration";
    case PLUGIN_CAP_LIGHT: return "light";
    case PLUGIN_CAP_COLOUR: return "colour";
    case PLUGIN_CAP_CURRENT: return "current";
    case PLUGIN_CAP_VOLTAGE: return "voltage";
    case PLUGIN_CAP_POWER: return "power";
    case PLUGIN_CAP_CLOCK: return "clock";
    case PLUGIN_CAP_DISPLAY: return "display";
    case PLUGIN_CAP_STORAGE: return "storage";
    case PLUGIN_CAP_BUTTON: return "button";
    case PLUGIN_CAP_ENCODER: return "encoder";
    case PLUGIN_CAP_MUX: return "mux";
    default: return "";
  }
}

inline uint32_t pluginCapFromName(const char *name) {
  if (!name) return 0;
  if (!strcmp(name, "digital.input")) return PLUGIN_CAP_DIGITAL_IN;
  if (!strcmp(name, "digital.output")) return PLUGIN_CAP_DIGITAL_OUT;
  if (!strcmp(name, "analog.input")) return PLUGIN_CAP_ANALOG_IN;
  if (!strcmp(name, "analog.output")) return PLUGIN_CAP_ANALOG_OUT;
  if (!strcmp(name, "pwm.output")) return PLUGIN_CAP_PWM_OUT;
  if (!strcmp(name, "servo.output")) return PLUGIN_CAP_SERVO_OUT;
  if (!strcmp(name, "temperature")) return PLUGIN_CAP_TEMPERATURE;
  if (!strcmp(name, "humidity")) return PLUGIN_CAP_HUMIDITY;
  if (!strcmp(name, "pressure")) return PLUGIN_CAP_PRESSURE;
  if (!strcmp(name, "distance")) return PLUGIN_CAP_DISTANCE;
  if (!strcmp(name, "motion")) return PLUGIN_CAP_MOTION;
  if (!strcmp(name, "orientation")) return PLUGIN_CAP_ORIENTATION;
  if (!strcmp(name, "acceleration")) return PLUGIN_CAP_ACCEL;
  if (!strcmp(name, "light")) return PLUGIN_CAP_LIGHT;
  if (!strcmp(name, "colour") || !strcmp(name, "color")) return PLUGIN_CAP_COLOUR;
  if (!strcmp(name, "current")) return PLUGIN_CAP_CURRENT;
  if (!strcmp(name, "voltage")) return PLUGIN_CAP_VOLTAGE;
  if (!strcmp(name, "power")) return PLUGIN_CAP_POWER;
  if (!strcmp(name, "clock")) return PLUGIN_CAP_CLOCK;
  if (!strcmp(name, "display")) return PLUGIN_CAP_DISPLAY;
  if (!strcmp(name, "storage")) return PLUGIN_CAP_STORAGE;
  if (!strcmp(name, "button")) return PLUGIN_CAP_BUTTON;
  if (!strcmp(name, "encoder")) return PLUGIN_CAP_ENCODER;
  if (!strcmp(name, "mux") || !strcmp(name, "i2c.mux")) return PLUGIN_CAP_MUX;
  return 0;
}

#endif
