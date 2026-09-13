#ifndef SHOWDUINO_EMERGENCY_NODE_LINK_H
#define SHOWDUINO_EMERGENCY_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"

struct EmergencyNodeStatus {
  bool used = false;
  bool seen = false;
  bool online = false;
  bool inputOpen = false;
  bool latched = false;
  bool acked = false;
  uint32_t lastRxMs = 0;
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = "";
  char mac[18] = "";
  char firmware[12] = "";
  char state[20] = "OFFLINE";
};

void emergencyNodeLinkBegin();
void emergencyNodeLinkLoop();
bool emergencyNodeLinkHandleReport(const char *line);
bool emergencyNodeLinkHandleCommand(const char *command, char *reply, size_t replyLen);
void emergencyNodeLinkOnEmergency(bool active);
void emergencyNodeLinkPublishToDirector();
uint8_t emergencyNodeLinkOnlineCount();
uint8_t emergencyNodeLinkSeenCount();
uint8_t emergencyNodeLinkOfflineCount();
uint8_t emergencyNodeLinkAssertingCount();
bool emergencyNodeLinkSafetyFault();
const EmergencyNodeStatus *emergencyNodeLinkFind(const char *id);
void emergencyNodeLinkAppendJsonArray(String &json);
void emergencyNodeLinkAppendDevicesJson(String &json, bool &first);
void emergencyNodeLinkPrimarySource(char *kind, size_t kindn,
                                    char *id, size_t idn,
                                    char *name, size_t namen);

#endif
