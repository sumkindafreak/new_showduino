#ifndef SHOWDUINO_EMERGENCY_NODE_LINK_H
#define SHOWDUINO_EMERGENCY_NODE_LINK_H

#include <Arduino.h>
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_station_blockers.h"

struct EmergencyNodeStatus {
  bool used = false;
  bool seen = false;
  bool online = false;
  bool inputOpen = false;
  /* MISSION 001H: an identity-verified ANNOUNCE was accepted this session, and the
   * L= value from that report. A bare ASSERT alone never sets a verified release. */
  bool reportKnown = false;
  bool announcedLatched = false;
  bool latched = false;
  bool acked = false;
  uint32_t syncMs = 0;
  bool pixelCapable = false;
  bool pixelReady = false;
  bool pixelEmergency = false;
  uint8_t pixelPin = 2;
  uint8_t pixelBrightness = 255;
  uint16_t pixelConfigured = 0;
  uint16_t pixelCount = 0;
  uint16_t pixelMax = 512;
  uint32_t lastRxMs = 0;
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1] = "";
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1] = "";
  char mac[18] = "";
  char firmware[12] = "";
  char state[20] = "OFFLINE";
  char pixelCaps[96] = "";
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
uint8_t emergencyNodeLinkPressedCount();
/* Clear-blocker restore and persistence (MISSION 001D). */
void emergencyNodeLinkStageRestore(const ShowduinoBlocker *list, uint8_t n,
                                   ShowduinoBlockerStatus status, bool latched,
                                   bool latchUnknown);
bool emergencyNodeLinkBlockersVerified();
bool emergencyNodeLinkPersistBlockers();
void emergencyNodeLinkRetryBlockers();
bool emergencyNodeLinkBlockerStorageFault();
int emergencyNodeLinkClearBlocked();
bool emergencyNodeLinkSafetyFault();
const EmergencyNodeStatus *emergencyNodeLinkFind(const char *id);
const EmergencyNodeStatus *emergencyNodeLinkSlot(uint8_t i);
void emergencyNodeLinkAppendJsonArray(String &json);
void emergencyNodeLinkAppendDevicesJson(String &json, bool &first);
void emergencyNodeLinkPrimarySource(char *kind, size_t kindn,
                                    char *id, size_t idn,
                                    char *name, size_t namen);

#endif
