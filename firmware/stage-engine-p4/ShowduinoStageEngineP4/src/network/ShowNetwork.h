#ifndef SHOWDUINO_SHOW_NETWORK_H
#define SHOWDUINO_SHOW_NETWORK_H

#include <Arduino.h>

/*
 * Optional P4 Ethernet show-network service.
 * Boot never waits for a cable, DHCP, router, internet, or E1.31.
 */

enum class ShowNetMode : uint8_t {
  Dhcp = 0,
  Static
};

struct ShowNetConfig {
  uint8_t formatVersion = 1;
  bool enabled = true;
  ShowNetMode mode = ShowNetMode::Dhcp;
  char ip[16] = "";
  char subnet[16] = "";
  char gateway[16] = "";
  char dns[16] = "";
  bool e131Enabled = true;
  uint16_t e131Universe = 1;
};

struct ShowNetLive {
  bool enabled = false;
  bool hardwareInit = false;
  bool linkUp = false;
  bool hasIp = false;
  ShowNetMode liveMode = ShowNetMode::Dhcp;
  char mac[18] = "";
  char ip[16] = "";
  char subnet[16] = "";
  char gateway[16] = "";
  char dns[16] = "";
  uint16_t speedMbps = 0;
  uint32_t linkUpMs = 0;
  uint32_t ipUpMs = 0;
  bool httpListening = false;
  char webUrl[40] = "";
  char lastError[48] = "";
  uint32_t beginHeap = 0;
  uint32_t lastEventMs = 0;
};

void showNetworkBegin();
void showNetworkLoop();
const ShowNetConfig &showNetworkSavedConfig();
const ShowNetLive &showNetworkLive();
bool showNetworkHasAddress();
bool showNetworkFullDuplex();
void showNetworkPrintStatus();
bool showNetworkHandleCommand(const String &command);
bool showNetworkValidateConfig(const ShowNetConfig &in, char *err, size_t errLen);
bool showNetworkSaveConfig(const ShowNetConfig &in, char *err, size_t errLen);
void showNetworkRequestApply();
const char *showNetModeName(ShowNetMode mode);
ShowNetMode showNetModeFromName(const char *name);

#endif
