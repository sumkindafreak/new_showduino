#ifndef SHOWDUINO_STAGE_CONFIG_H
#define SHOWDUINO_STAGE_CONFIG_H

#include <Arduino.h>

struct StageSystemConfig {
  uint8_t formatVersion = 1;
  char systemName[32] = "Showduino";
  char lastLoadedProductionId[48] = "";
  char bootBehaviour[16] = "idle";
  bool diagnosticsVerbose = false;
  bool featureEthernet = true;
  bool featureE131Receive = true;
};

struct StageE131Persist {
  uint8_t formatVersion = 1;
  bool enabled = true;
  uint16_t universe = 1;
  bool multicast = true;
  uint16_t sourceTimeoutMs = 2500;
};

struct StagePixelsConfig {
  uint8_t formatVersion = 1;
  bool enabled = false;
  int16_t gpio = 23;
  uint16_t pixelCount = 0;
  char colourOrder[8] = "GRB";
  uint8_t brightnessLimit = 128;
  char defaultState[8] = "off";
};

struct StageDirectorConfig {
  uint8_t formatVersion = 1;
  uint16_t statusPublishMs = 1000;
};

void stageConfigBegin();
const StageSystemConfig &stageConfigSystem();
const StageE131Persist &stageConfigE131();
const StagePixelsConfig &stageConfigPixels();
const StageDirectorConfig &stageConfigDirector();
bool stageConfigSaveSystem();
bool stageConfigSaveE131();
void stageConfigSetLastProduction(const char *id);
void stageConfigApplyE131To(bool *enabled, uint16_t *universe);
void stageConfigTakeE131From(bool enabled, uint16_t universe);
bool stageConfigFileHealthy(const char *path, size_t maxBytes);

#endif
