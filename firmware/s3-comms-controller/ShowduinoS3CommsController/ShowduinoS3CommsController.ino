/*
  Showduino ESP32-S3 Comms Controller

  Role: communications processor only.
    Director --ESP-NOW--> this S3 --UART 115200 8N1--> ESP32-P4 Stage Engine

  Phase 1: ESP-NOW <-> UART bridge. No BLE, Wi-Fi networking, WebUI, or OTA.
*/

#include <Arduino.h>
#ifdef ESP_ARDUINO_VERSION_STR
#include <esp_arduino_version.h>
#endif
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "BoardConfig.h"
#include "src/CommsUart.h"
#include "src/EspNowTransport.h"
#include "src/ProtocolBridge.h"
#include "src/CommsConsole.h"

static void commsLogFlush(const char *line) {
  Serial.println(line);
  Serial.flush();
}

void setup() {
  /* USB-powered S3 Dev Modules often sag when UART/radio start.
   * Disable the reset-on-brownout detector so a short dip does not reboot-loop.
   * Still use a decent 5 V supply; unplug P4 UART wires for the first boot. */
  CLEAR_PERI_REG_MASK(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);

  Serial.begin(USB_DEBUG_BAUD);
  delay(800);
  Serial.flush();

  Serial.println();
  Serial.println("============================================================");
  Serial.println(" SHOWDUINO ESP32-S3 COMMS CONTROLLER");
  Serial.println("============================================================");
  Serial.printf("[COMMS] Firmware: %s\n", SHOWDUINO_COMMS_FIRMWARE_VERSION);
#ifdef ESP_ARDUINO_VERSION_STR
  Serial.printf("[COMMS] Arduino: %s\n", ESP_ARDUINO_VERSION_STR);
#else
  Serial.println("[COMMS] Arduino: (version macro unavailable)");
#endif
  Serial.printf("[COMMS] IDF: %s\n", ESP.getSdkVersion());
  Serial.printf("[COMMS] UART RX=%d TX=%d baud=%u\n",
                SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN,
                (unsigned)SHOWDUINO_COMMS_UART_BAUD);
  Serial.flush();

  commsLogFlush("[COMMS] Initialising UART1");
  protocolBridgeBegin();
  commsConsoleBegin();
  commsLogFlush("[COMMS] UART1 ready");

  commsLogFlush("[COMMS] Initialising ESP-NOW radio");
  if (espNowTransportBegin()) {
    commsLogFlush("[COMMS] ESP-NOW initialised");
  } else {
    commsLogFlush("[COMMS] ESP-NOW failed — UART DIAG:PING/PONG still available");
  }

  uint8_t mac[6] = {0};
  const bool haveMac = espNowTransportReadStaMac(mac);
  Serial.print("[COMMS] Wi-Fi MAC: ");
  if (haveMac) {
    espNowTransportPrintMac(mac);
    Serial.println();
  } else {
    Serial.println("(unavailable)");
  }
  Serial.print("[COMMS] Wi-Fi MAC=");
  if (haveMac) {
    espNowTransportPrintMac(mac);
    Serial.println();
  } else {
    Serial.println("(unavailable)");
  }
  Serial.flush();

  if (!haveMac || (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
                   mac[3] == 0 && mac[4] == 0 && mac[5] == 0)) {
    Serial.println("[COMMS] WARNING: MAC is all zeros — Wi-Fi radio not ready.");
  } else {
    Serial.println("[COMMS] Copy this MAC into Director BoardConfig.h SHOWDUINO_COMMS_MAC_*");
  }

  Serial.println("[COMMS] Waiting for Director");
  Serial.println("[COMMS] Waiting for P4");
  Serial.println("[COMMS] Ready");
  Serial.println("[COMMS] Type HELP for USB maintenance commands.");
  Serial.flush();
}

void loop() {
  commsConsoleLoop();
  protocolBridgeLoop();
  delay(5);
}
