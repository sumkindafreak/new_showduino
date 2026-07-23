#ifndef SHOWDUINO_WEBSOCKET_MANAGER_H
#define SHOWDUINO_WEBSOCKET_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "DeviceRecord.h"
#include "NetworkStatistics.h"

class WebSocketManager {
 public:
  void begin(uint16_t port = 81);
  void loop();
  bool ready() const { return ready_; }

  void broadcastText(const String &message);
  void sendDeviceEvent(const char *eventName, const DeviceRecord &device);
  void sendNetworkStats(const NetworkStatistics &stats);
  void sendSnapshot(const String &devicesJson, const String &networkJson);
  void sendCommandEvent(const char *eventName, const String &commandJson);
  void sendQueueUpdated(size_t depth, size_t emergencyDepth);
  void sendJsonEvent(const char *eventName, const char *detailJson);

 private:
  static const size_t MAX_CLIENTS = 4;
  struct Client {
    WiFiClient sock;
    bool open = false;
  };

  bool ready_ = false;
  WiFiServer *server_ = nullptr;
  Client clients_[MAX_CLIENTS];

  void acceptNew();
  void serviceClient(size_t idx);
  bool doHandshake(WiFiClient &client, const String &req);
  void sendFrame(WiFiClient &client, const String &payload);
  static String makeAcceptKey(const String &clientKey);
};

extern WebSocketManager gWebSocketManager;

#endif