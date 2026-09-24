#ifndef SHOWDUINO_AUDIO_NODE_LINK_H
#define SHOWDUINO_AUDIO_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_audio_node.h"

struct AudioNodeStatus {
  bool seen = false;
  bool online = false;
  bool pending = false;
  uint8_t volume = 80;
  uint32_t lastSeq = 0;
  uint32_t pendingSeq = 0;
  uint32_t lastRxMs = 0;
  uint16_t inventoryTotal = 0;
  uint16_t inventoryPage = 0;
  char mac[18] = "";
  char firmware[12] = "";
  char state[16] = "OFFLINE";
  char asset[64] = "";
  char lastLife[16] = "";
  char lastError[24] = "";
  char capabilities[128] = SHOWDUINO_AUDIO_CAPS;
  char codec[12] = "ES8388";
  char storage[16] = "UNKNOWN";
  char output[12] = "SPEAKER";
  char inventory[SHOWDUINO_AUDIO_INV_PER_PAGE][40] = {};
  bool soundReady = false;
  bool soundArmed = false;
  bool soundCalibrated = false;
  bool soundEnabled = false;
  uint8_t soundLevel = 0;
  uint8_t soundPeak = 0;
  uint8_t soundFloor = 0;
  uint8_t soundThreshold = 0;
  uint16_t soundCooldown = 0;
  uint32_t soundLastId = 0;
  uint32_t soundLastMs = 0;
  char soundReadyTok[6] = "OFF";
  char soundLastType[12] = "NONE";
  char logicalInput[32] = "";
  char logicalSubtype[12] = "";
};

void audioNodeLinkBegin();
void audioNodeLinkLoop();
bool audioNodeLinkHandleReport(const char *line);
bool audioNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen);
void audioNodeLinkOnEmergency(bool active);
void audioNodeLinkPublishToDirector();
const AudioNodeStatus &audioNodeLinkStatus();
void audioNodeLinkAppendJson(String &json);

#endif
