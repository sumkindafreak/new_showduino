#include "AudioInputConfig.h"
#include "../AudioStorage.h"
#include "../../BoardConfig.h"

#include <SD.h>

static ShowduinoSoundInputConfig sCfg;
static bool sFault = false;

void audioInputConfigBegin() {
  showduino_sound_config_defaults(&sCfg);
  sFault = false;
}

const ShowduinoSoundInputConfig &audioInputConfig() { return sCfg; }

ShowduinoAudioCfgStatus audioInputConfigApplyJson(const char *json) {
  ShowduinoSoundInputConfig parsed;
  const ShowduinoAudioCfgStatus st = showduino_sound_config_parse(json, &parsed);
  if (st != SHOWDUINO_AUDIO_CFG_OK) {
    sFault = true;
    showduino_sound_config_defaults(&sCfg);
    return st;
  }
  sCfg = parsed;
  sFault = false;
  return SHOWDUINO_AUDIO_CFG_OK;
}

void audioInputConfigFormatJson(char *out, size_t n) {
  if (!out || n == 0) return;
  snprintf(out, n,
           "    \"enabled\": %s,\n"
           "    \"mode\": \"%s\",\n"
           "    \"threshold\": %u,\n"
           "    \"thresholdAboveNoiseFloor\": %u,\n"
           "    \"minimumDurationMs\": %u,\n"
           "    \"sustainedDurationMs\": %u,\n"
           "    \"quietDurationMs\": %u,\n"
           "    \"cooldownMs\": %u,\n"
           "    \"hysteresis\": %u,\n"
           "    \"postPlaybackInhibitMs\": %u,\n"
           "    \"triggerWhilePlaying\": %s,\n"
           "    \"autoNoiseFloor\": %s",
           sCfg.enabled ? "true" : "false",
           showduino_sound_mode_name(sCfg.mode),
           (unsigned)sCfg.threshold,
           (unsigned)sCfg.thresholdAboveNoiseFloor,
           (unsigned)sCfg.minimumDurationMs,
           (unsigned)sCfg.sustainedDurationMs,
           (unsigned)sCfg.quietDurationMs,
           (unsigned)sCfg.cooldownMs,
           (unsigned)sCfg.hysteresis,
           (unsigned)sCfg.postPlaybackInhibitMs,
           sCfg.triggerWhilePlaying ? "true" : "false",
           sCfg.autoNoiseFloor ? "true" : "false");
}

void audioInputConfigSet(const ShowduinoSoundInputConfig *cfg, bool persist) {
  if (!cfg) return;
  sCfg = *cfg;
  if (persist) audioStorageWriteConfig();
}

void audioInputConfigSetEnabled(bool on, bool persist) {
  sCfg.enabled = on ? 1 : 0;
  if (persist) audioStorageWriteConfig();
}

void audioInputConfigSetThreshold(uint8_t threshold, bool persist) {
  if (threshold > 100) threshold = 100;
  sCfg.threshold = threshold;
  if (persist) audioStorageWriteConfig();
}

void audioInputConfigSetCooldown(uint16_t ms, bool persist) {
  if (ms < 200) ms = 200;
  if (ms > 30000) ms = 30000;
  sCfg.cooldownMs = ms;
  if (persist) audioStorageWriteConfig();
}

void audioInputConfigSetInhibit(uint16_t ms, bool persist) {
  if (ms > 10000) ms = 10000;
  sCfg.postPlaybackInhibitMs = ms;
  if (persist) audioStorageWriteConfig();
}

void audioInputConfigSetMode(uint8_t mode, bool persist) {
  sCfg.mode = mode;
  if (persist) audioStorageWriteConfig();
}

bool audioInputConfigLoadCal(uint8_t *noiseFloor) {
  if (!audioStorageReady() || !SD.exists(PATH_AUDIO_CAL)) return false;
  File f = SD.open(PATH_AUDIO_CAL, FILE_READ);
  if (!f) return false;
  String json = f.readString();
  f.close();
  const char *p = showduino_audio_json_after_key(json.c_str(), "noiseFloor");
  if (!p) return false;
  int v = 0;
  if (!showduino_audio_parse_u32(p, 3, &v) || v > 100) return false;
  if (noiseFloor) *noiseFloor = (uint8_t)v;
  return true;
}

bool audioInputConfigSaveCal(uint8_t noiseFloor) {
  if (!audioStorageWritable()) return false;
  File f = SD.open(PATH_AUDIO_CAL, FILE_WRITE);
  if (!f) return false;
  f.printf("{\"noiseFloor\":%u}\n", (unsigned)noiseFloor);
  f.close();
  return true;
}

bool audioInputConfigFault() { return sFault; }
