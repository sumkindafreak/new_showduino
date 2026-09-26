#ifndef SHOWDUINO_MOSFET_NODE_LINK_H
#define SHOWDUINO_MOSFET_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_mosfet_node.h"

struct MosfetNodeStatus {
  bool used = false;
  bool seen = false;
  bool online = false;
  bool owned = false;
  bool emergency = false;
  uint8_t levels[SHOWDUINO_MOSFET_OUT_COUNT] = {0, 0, 0, 0};
  uint32_t lastSeq = 0;
  uint32_t lastRxMs = 0;
  char id[SHOWDUINO_MOSFET_ID_MAX + 1] = "";
  char name[SHOWDUINO_MOSFET_NAME_MAX + 1] = "";
  char outNames[SHOWDUINO_MOSFET_OUT_COUNT][SHOWDUINO_MOSFET_OUT_NAME_MAX + 1] = {
      "OUT1", "OUT2", "OUT3", "OUT4"};
  char mac[18] = "";
  char firmware[12] = "";
  char state[20] = "OFFLINE";
  char capabilities[64] = "";
  char lastError[24] = "";
};

void mosfetNodeLinkBegin();
void mosfetNodeLinkLoop();
bool mosfetNodeLinkHandleReport(const char *line);
bool mosfetNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen);
void mosfetNodeLinkOnEmergency(bool active);
void mosfetNodeLinkAllOff(const char *reason);
void mosfetNodeLinkPublishToDirector();
uint8_t mosfetNodeLinkOnlineCount();
uint8_t mosfetNodeLinkSeenCount();
const MosfetNodeStatus *mosfetNodeLinkFind(const char *id);
const MosfetNodeStatus *mosfetNodeLinkSlot(uint8_t i);
void mosfetNodeLinkAppendJsonArray(String &json);
void mosfetNodeLinkAppendDevicesJson(String &json, bool &first);

#endif
