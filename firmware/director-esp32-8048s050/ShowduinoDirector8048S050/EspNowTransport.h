#ifndef SHOWDUINO_ESPNOW_TRANSPORT_H
#define SHOWDUINO_ESPNOW_TRANSPORT_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "BoardConfig.h"

// =========================================================
// Showduino ESP-NOW transport
// Portable Director S3 -> P4 board built-in ESP32-C6 wireless bridge
// =========================================================

struct ShowduinoEspNowPacket {
  uint32_t magic;
  uint16_t version;
  uint16_t sequence;
  uint32_t sentMillis;
  char command[SHOWDUINO_ESPNOW_COMMAND_MAX];
};

class ShowduinoEspNowTransport {
public:
  bool begin() {
    Serial.println("ESP-NOW: starting portable Director transport...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);

    Serial.print("ESP-NOW: Director MAC = ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
      Serial.println("ESP-NOW: init failed.");
      online = false;
      return false;
    }

    esp_now_register_send_cb(onSentStatic);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, stageBridgeMac, 6);
    peerInfo.channel = SHOWDUINO_ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("ESP-NOW: failed to add P4/C6 bridge peer.");
      online = false;
      return false;
    }

    online = true;
    Serial.print("ESP-NOW: P4/C6 bridge peer = ");
    printMac(stageBridgeMac);
    Serial.println();
    return true;
  }

  bool sendCommand(const String &command) {
    if (!online) return false;

    ShowduinoEspNowPacket packet = {};
    packet.magic = SHOWDUINO_ESPNOW_MAGIC;
    packet.version = SHOWDUINO_ESPNOW_VERSION;
    packet.sequence = nextSequence++;
    packet.sentMillis = millis();
    command.substring(0, SHOWDUINO_ESPNOW_COMMAND_MAX - 1).toCharArray(packet.command, SHOWDUINO_ESPNOW_COMMAND_MAX);

    esp_err_t result = esp_now_send(stageBridgeMac, (uint8_t *)&packet, sizeof(packet));
    lastSendOk = (result == ESP_OK);
    lastCommand = command;
    lastSequence = packet.sequence;

    if (!lastSendOk) {
      Serial.print("ESP-NOW: send failed for command: ");
      Serial.println(command);
    }

    return lastSendOk;
  }

  bool isOnline() const { return online; }
  bool wasLastSendOk() const { return lastSendOk; }
  uint16_t getLastSequence() const { return lastSequence; }
  String getLastCommand() const { return lastCommand; }

private:
  bool online = false;
  bool lastSendOk = false;
  uint16_t nextSequence = 1;
  uint16_t lastSequence = 0;
  String lastCommand;

  uint8_t stageBridgeMac[6] = {
    SHOWDUINO_P4_C6_MAC_0,
    SHOWDUINO_P4_C6_MAC_1,
    SHOWDUINO_P4_C6_MAC_2,
    SHOWDUINO_P4_C6_MAC_3,
    SHOWDUINO_P4_C6_MAC_4,
    SHOWDUINO_P4_C6_MAC_5
  };

  static void onSentStatic(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.print("ESP-NOW: send status = ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "delivered" : "failed");
  }

  void printMac(const uint8_t *mac) {
    for (uint8_t i = 0; i < 6; i++) {
      if (i > 0) Serial.print(":");
      if (mac[i] < 16) Serial.print("0");
      Serial.print(mac[i], HEX);
    }
  }
};

#endif
