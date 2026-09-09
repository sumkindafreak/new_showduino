#ifndef SHOWDUINO_RADIO_FOLLOW_H
#define SHOWDUINO_RADIO_FOLLOW_H

#include "showduino_radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

/*
 * Header-only helper for Director / Audio / Lamp.
 * Include from exactly one translation unit per firmware.
 * Scan only while unlinked. Never deinit ESP-NOW here.
 */

class ShowduinoRadioFollow {
public:
  void begin(uint8_t home = SHOWDUINO_RADIO_HOME_CHANNEL) {
    channel_ = home ? home : SHOWDUINO_RADIO_HOME_CHANNEL;
    nextScanMs_ = millis() + SHOWDUINO_RADIO_FOLLOW_BOOT_MS;
    backoffMs_ = SHOWDUINO_RADIO_FOLLOW_RETRY_MIN_MS;
    scanning_ = false;
  }

  uint8_t channel() const { return channel_; }
  bool scanning() const { return scanning_; }

  void lock(uint8_t ch) {
    if (ch < SHOWDUINO_RADIO_CHANNEL_MIN || ch > SHOWDUINO_RADIO_CHANNEL_MAX) {
      ch = SHOWDUINO_RADIO_HOME_CHANNEL;
    }
    channel_ = ch;
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    (void)esp_wifi_scan_stop();
    (void)esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
  }

  /* haveLink: recently heard Comms on ESP-NOW. */
  void tick(bool haveLink) {
    if (haveLink) {
      uint8_t ch = 0;
      wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
      esp_wifi_get_channel(&ch, &second);
      if (ch >= SHOWDUINO_RADIO_CHANNEL_MIN && ch <= SHOWDUINO_RADIO_CHANNEL_MAX) {
        channel_ = ch;
      }
      scanning_ = false;
      backoffMs_ = SHOWDUINO_RADIO_FOLLOW_RETRY_MIN_MS;
      nextScanMs_ = millis() + SHOWDUINO_RADIO_FOLLOW_RETRY_MIN_MS;
      return;
    }

    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    if (n >= 0) {
      applyScan(n);
      WiFi.scanDelete();
      return;
    }

    const uint32_t now = millis();
    if ((int32_t)(now - nextScanMs_) < 0) return;
    nextScanMs_ = now + backoffMs_;
    if (backoffMs_ < SHOWDUINO_RADIO_FOLLOW_RETRY_MAX_MS) {
      backoffMs_ += 3000UL;
    }
    startScan();
  }

private:
  uint8_t channel_ = SHOWDUINO_RADIO_HOME_CHANNEL;
  uint32_t nextScanMs_ = 0;
  uint32_t backoffMs_ = SHOWDUINO_RADIO_FOLLOW_RETRY_MIN_MS;
  bool scanning_ = false;

  void startScan() {
    scanning_ = true;
    (void)WiFi.scanNetworks(true, false, false, 120);
  }

  void applyScan(int16_t count) {
    scanning_ = false;
    int8_t bestRssi = -127;
    uint8_t bestCh = 0;
    for (int16_t i = 0; i < count; ++i) {
      if (WiFi.SSID(i) != SHOWDUINO_RADIO_AP_SSID) continue;
      if (WiFi.RSSI(i) >= bestRssi) {
        bestRssi = WiFi.RSSI(i);
        bestCh = (uint8_t)WiFi.channel(i);
      }
    }
    if (bestCh >= SHOWDUINO_RADIO_CHANNEL_MIN &&
        bestCh <= SHOWDUINO_RADIO_CHANNEL_MAX) {
      Serial.printf("[RADIO] follow Showduino AP ch%u rssi=%d\n",
                    (unsigned)bestCh, (int)bestRssi);
      lock(bestCh);
    }
  }
};

#endif /* SHOWDUINO_RADIO_FOLLOW_H */
