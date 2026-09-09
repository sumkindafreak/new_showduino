#include "CommsConsole.h"
#include "CommsUart.h"
#include "EspNowTransport.h"
#include "ProtocolBridge.h"
#include "web/CommsWebServer.h"
#include "status/CommsStatusRgb.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_log.h"

#ifdef ESP_ARDUINO_VERSION_STR
#include <esp_arduino_version.h>
#endif

static String sUsb;

static void printHelp() {
  Serial.println("Commands: HELP  STATUS  MAC  PING:P4  RGB:STATUS  RGB:TEST");
  Serial.println("  LOG:LEVEL  LOG:LEVEL:ERROR|WARN|INFO|DEBUG|TRACE");
  Serial.println("USB does not inject Stage Engine commands.");
  Serial.println("WebUI SoftAP SSID " SHOWDUINO_WEBUI_AP_SSID "  http://192.168.4.1/");
}

static void printMacLine() {
  uint8_t mac[6] = {0};
  Serial.print("[COMMS] Wi-Fi MAC: ");
  if (espNowTransportReadStaMac(mac)) {
    espNowTransportPrintMac(mac);
    Serial.println();
  } else {
    Serial.println("(unavailable)");
  }
}

static void printStatus() {
  Serial.println("--- Showduino S3 Comms Controller ---");
  Serial.printf("Firmware: %s\n", SHOWDUINO_COMMS_FIRMWARE_VERSION);
#ifdef ESP_ARDUINO_VERSION_STR
  Serial.printf("Arduino: %s\n", ESP_ARDUINO_VERSION_STR);
#endif
  Serial.printf("IDF: %s\n", ESP.getSdkVersion());
  Serial.printf("Uptime: %lu ms\n", (unsigned long)millis());
  Serial.printf("Heap: %lu\n", (unsigned long)ESP.getFreeHeap());
  Serial.printf("ESP-NOW: %s\n", espNowTransportReady() ? "initialised" : "FAILED");
  Serial.printf("Director: %s\n",
                protocolBridgeDirectorOnline() ? "ONLINE" :
                (espNowTransportHaveDirector() ? "seen (stale)" : "not seen"));
  if (espNowTransportHaveDirector()) {
    uint8_t dmac[6] = {0};
    espNowTransportDirectorMac(dmac);
    Serial.print("Director MAC: ");
    espNowTransportPrintMac(dmac);
    Serial.println();
  }
  Serial.printf("P4 UART: %s\n",
                protocolBridgeP4Alive() ? "ALIVE" :
                (commsUartEverRx() ? "seen (stale)" : "not seen"));
  if (commsUartEverRx()) {
    Serial.printf("Last P4 RX age: %lu ms\n",
                  (unsigned long)(millis() - commsUartLastRxMs()));
  } else {
    Serial.println("Last P4 RX age: n/a");
  }
  Serial.printf("UART RX/TX: %lu / %lu\n",
                (unsigned long)commsUartRxCount(),
                (unsigned long)commsUartTxCount());
  Serial.printf("ESP-NOW RX/TX: %lu / %lu\n",
                (unsigned long)espNowTransportRxCount(),
                (unsigned long)espNowTransportTxCount());
  Serial.printf("Rejected/dropped: ESP-NOW=%lu UART=%lu\n",
                (unsigned long)espNowTransportRejectedCount(),
                (unsigned long)commsUartDroppedCount());
#if SHOWDUINO_WEBUI_ENABLED
  Serial.printf("WebUI: %s  SSID=%s  IP=%s  radio ch=%u\n",
                commsWebReady() ? "ready" : "down",
                commsWebSsid(),
                commsWebIp(),
                (unsigned)commsWebRadioChannel());
#endif
  Serial.printf("Status RGB: %s %s pin=%u\n",
                commsStatusRgbStateName(),
                commsStatusRgbColourName(),
                (unsigned)commsStatusRgbPin());
}

static void handleUsbLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "HELP") {
    printHelp();
    return;
  }
  if (showduino_log_handle_command(line.c_str())) return;
  if (line == "STATUS") {
    printStatus();
    return;
  }
  if (line == "MAC") {
    printMacLine();
    return;
  }
  if (line == "PING:P4") {
    protocolBridgePingP4();
    return;
  }
  if (line == "RGB:STATUS") {
    commsStatusRgbPrintStatus();
    return;
  }
  if (line == "RGB:TEST") {
    commsStatusRgbStartTest();
    return;
  }

  Serial.println("[COMMS] Unknown USB command. Type HELP.");
}

void commsConsoleBegin() {
  sUsb = "";
}

void commsConsoleLoop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (sUsb.length() > 0) {
        handleUsbLine(sUsb);
        sUsb = "";
      }
    } else {
      sUsb += c;
      if (sUsb.length() > 180) {
        sUsb = "";
        Serial.println("[COMMS] USB command too long; buffer cleared");
      }
    }
  }
}
