#ifndef SHOWDUINO_PIXEL_NODE_LINK_H
#define SHOWDUINO_PIXEL_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_pixel_node.h"

struct PixelNodeStatus {
  bool used = false;
  bool seen = false;
  bool online = false;
  bool initialised = false;
  bool pending = false;
  uint8_t segments = 0;
  uint16_t pixelCount = 0;
  uint32_t lastSeq = 0;
  uint32_t pendingSeq = 0;
  uint32_t lastRxMs = 0;
  char id[SHOWDUINO_PIXEL_ID_MAX + 1] = "";
  char name[SHOWDUINO_PIXEL_NAME_MAX + 1] = "";
  char mac[18] = "";
  char firmware[12] = "";
  char state[20] = "OFFLINE";
  char lastLife[16] = "";
  char lastError[24] = "";
};

void pixelNodeLinkBegin();
void pixelNodeLinkLoop();
bool pixelNodeLinkHandleReport(const char *line);
bool pixelNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen);
void pixelNodeLinkOnEmergency(bool active);
void pixelNodeLinkPublishToDirector();
uint8_t pixelNodeLinkOnlineCount();
uint8_t pixelNodeLinkSeenCount();
const PixelNodeStatus *pixelNodeLinkFind(const char *id);
void pixelNodeLinkAppendJsonArray(String &json);
void pixelNodeLinkAppendDevicesJson(String &json, bool &first);

#endif
