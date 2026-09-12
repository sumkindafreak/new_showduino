#ifndef SHOWDUINO_NODE_SOFTAP_H
#define SHOWDUINO_NODE_SOFTAP_H

#include <Arduino.h>
#include "../../../protocol/showduino_node_ownership.h"

typedef void (*NodeSoftApRadioHook)();

bool nodeSoftApBegin(const char *typeToken, const uint8_t mac[6],
                     uint8_t channel, const char *password);
bool nodeSoftApBeginNamed(const char *ssid, uint8_t channel, const char *password);
void nodeSoftApRetitle(const char *ssid);
void nodeSoftApSetRadioHook(NodeSoftApRadioHook fn);
void nodeSoftApFollowChannel(uint8_t channel);
void nodeSoftApService();
void nodeSoftApLockChannel();
bool nodeSoftApReady();
bool nodeSoftApStarted();
const char *nodeSoftApSsid();
const char *nodeSoftApIp();
const char *nodeSoftApPassword();
uint8_t nodeSoftApChannel();

#endif
