#ifndef SHOWDUINO_LAMP_NODE_LINK_H
#define SHOWDUINO_LAMP_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_lamp_node.h"

struct LampNodeStatus {
  bool seen = false;
  bool online = false;
  bool pending = false;
  bool fxActive = false;
  uint8_t brightness = 100;
  uint32_t lastSeq = 0;
  uint32_t pendingSeq = 0;
  uint32_t lastRxMs = 0;
  char mac[18] = "";
  char firmware[12] = "";
  char state[20] = "OFFLINE";
  char fx[24] = "-";
  char lastLife[16] = "";
  char lastError[24] = "";
};

void lampNodeLinkBegin();
void lampNodeLinkLoop();
bool lampNodeLinkHandleReport(const char *line);
bool lampNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen);
void lampNodeLinkOnEmergency(bool active);
void lampNodeLinkPublishToDirector();
const LampNodeStatus &lampNodeLinkStatus();
void lampNodeLinkAppendJson(String &json);

#endif
