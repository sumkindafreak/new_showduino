#ifndef SHOWDUINO_E131_RECEIVER_H
#define SHOWDUINO_E131_RECEIVER_H

#include <Arduino.h>
#include "../../../protocol/showduino_e131.h"

enum class E131RxState : uint8_t {
  Unavailable = 0,
  Offline,
  Stale,
  Online
};

struct E131RxStatus {
  bool enabled = false;
  E131RxState state = E131RxState::Unavailable;
  uint16_t universe = 1;
  char multicast[16] = "";
  bool multicastJoined = false;
  bool socketOpen = false;
  char sourceName[65] = "";
  char cid[48] = "";
  uint8_t priority = 0;
  uint8_t lastSequence = 0;
  bool haveSequence = false;
  uint32_t packetsOk = 0;
  uint32_t packetsRejected = 0;
  uint32_t lastPacketMs = 0;
  uint32_t lastAgeMs = 0;
  float rateFps = 0;
  uint32_t lastLoopUs = 0;
  uint32_t maxLoopUs = 0;
  uint32_t lastRejectReason = 0;
  char lastRejectName[28] = "";
};

void e131ReceiverBegin();
void e131ReceiverLoop();
void e131ReceiverOnNetworkChange();
void e131ReceiverStop();
const E131RxStatus &e131ReceiverStatus();
const uint8_t *e131ReceiverSlots();
void e131ReceiverCopySlots(uint8_t *out, uint16_t from, uint16_t count);
void e131ReceiverPrintStatus();
void e131ReceiverPrintChannels(uint16_t from, uint16_t count);
bool e131ReceiverHandleCommand(const String &command);
const char *e131RxStateName(E131RxState state);

#endif
