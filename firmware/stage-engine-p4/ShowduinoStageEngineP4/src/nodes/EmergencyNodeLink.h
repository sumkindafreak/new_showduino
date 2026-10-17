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
  /* MISSION 001I: in the current global-latch episode (any ASSERT, or an announced
   * latched station). Keeps the station in the stored clear-blocker set until an
   * accepted Director clear ends the episode. */
  bool asserted = false;
  bool latched = false;
  bool acked = false;
  /* MISSION 001J: from the last verified AUTH. ackedReport = the station holds the
   * current episode; proofChalSeq = challenge sequence that AUTH answered. */
  bool ackedReport = false;
  uint32_t proofChalSeq = 0;
  uint32_t ackMs = 0;
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
void emergencyNodeLinkEndEpisode();
void emergencyNodeLinkRetryBlockers();
bool emergencyNodeLinkBlockerStorageFault();
/* MISSION 001J: returns a ShowduinoClearDecision (0 = clear permitted).
 *   confirm=false (CLEAR_REQUEST): station and storage blockers only.
 *   confirm=true  (CLEAR_CONFIRM, USB clear): also every provisioned station must
 *   have a fresh verified RELEASED proof, and the GCL command counters are reserved
 *   (persisted) before this returns 0. */
int emergencyNodeLinkClearBlocked(bool confirm);
const char *emergencyNodeLinkClearReason(int decision);
/* CLEAR_REQUEST accepted: proofs must answer challenges issued from now on. */
void emergencyNodeLinkOnClearRequest();
/* Once at boot, before any challenge. Seed from the SAR ADC entropy source. */
void emergencyNodeLinkSeedNonce(const uint8_t seed[32]);
/* EMERGENCY:KEY:* / EMERGENCY:STATION:* / EMERGENCY:AUTH:*. Returns true when the
 * command belongs to this module. Refused unless localUsb. Never logs key bytes. */
bool emergencyNodeLinkHandleMaintenance(const char *command, bool localUsb);
bool emergencyNodeLinkIsMaintenanceCommand(const char *command);
bool emergencyNodeLinkSafetyFault();
const EmergencyNodeStatus *emergencyNodeLinkFind(const char *id);
const EmergencyNodeStatus *emergencyNodeLinkSlot(uint8_t i);
void emergencyNodeLinkAppendJsonArray(String &json);
void emergencyNodeLinkAppendDevicesJson(String &json, bool &first);
void emergencyNodeLinkPrimarySource(char *kind, size_t kindn,
                                    char *id, size_t idn,
                                    char *name, size_t namen);

#endif
