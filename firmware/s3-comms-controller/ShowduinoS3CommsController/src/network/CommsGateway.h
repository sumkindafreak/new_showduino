#ifndef SHOWDUINO_COMMS_GATEWAY_H
#define SHOWDUINO_COMMS_GATEWAY_H

#include <Arduino.h>

enum CommsWifiMode : uint8_t {
  COMMS_WIFI_AP_ONLY = 0,
  COMMS_WIFI_AP_STA = 1
};

enum CommsUpdateStatus : uint8_t {
  COMMS_UPDATE_NEVER = 0,
  COMMS_UPDATE_CURRENT,
  COMMS_UPDATE_AVAILABLE,
  COMMS_UPDATE_OFFLINE,
  COMMS_UPDATE_FAILED,
  COMMS_UPDATE_NONE
};

void commsGatewayBegin();
void commsGatewayLoop();

bool commsGatewayStaAssociated();
uint8_t commsGatewayTargetChannel();
uint8_t commsGatewayRadioChannel();
bool commsGatewayApOnline();
bool commsGatewayInternetOnline();
CommsWifiMode commsGatewayMode();
const char *commsGatewayStaSsid();
const char *commsGatewayStaIp();
int commsGatewayRssi();
bool commsGatewayPasswordConfigured();

bool commsGatewaySetMode(CommsWifiMode mode);
bool commsGatewayConnect(const char *ssid, const char *password);
void commsGatewayDisconnect();
void commsGatewayForget();

bool commsGatewayScanStart();
const char *commsGatewayScanJson(); /* {"state":"...","networks":[...]} */

void commsGatewayRequestUpdateCheck();
void commsGatewayAppendStatusJson(String &json);
void commsGatewayUpdatesJson(String &json);
void commsGatewayLogRadio(const char *reason);
void commsGatewayPushDirectorWires();

#endif
