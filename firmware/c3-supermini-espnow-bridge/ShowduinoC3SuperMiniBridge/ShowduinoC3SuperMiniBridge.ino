/*
 * Showduino Communications Engine — ESP32-C3 SuperMini (SUE)
 *
 * SoftAP front door for Showduino Studio WebUI.
 * Join: Showduino-Studio / showduino
 * Open: http://192.168.4.1/
 *
 * Optional: UART to P4 (GPIO20 RX / GPIO21 TX) for /api tunnel.
 */

#include <Arduino.h>
#include "BoardConfig.h"
#include "src/WebServerManager.h"
#include "src/P4WebTunnel.h"
#include "src/WebSocketManager.h"
#include "src/DeskUartBridge.h"

#if SHOWDUINO_WEBUI_ENABLED
#include <WiFi.h>
#endif

static unsigned long bootMs = 0;

#if SHOWDUINO_WEBUI_ENABLED
static String sP4LineBuf;

/* Feed WEBR: headers as lines, then body bytes into the tunnel parser. */
static void pumpP4Uart() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (p4WebTunnelConsumingBytes()) {
      p4WebTunnelOnByte(c);
      continue;
    }
    if (c == '\n' || c == '\r') {
      if (sP4LineBuf.length() > 0) {
        if (!p4WebTunnelOnLine(sP4LineBuf)) {
          deskUartBridgeOnP4Line(sP4LineBuf);
        }
        sP4LineBuf = "";
      }
    } else if (sP4LineBuf.length() < 512) {
      sP4LineBuf += c;
    }
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(400);
  bootMs = millis();

  Serial.println();
  Serial.println("Showduino C3 Communications Engine (SUE)");
  Serial.printf("FW %s\n", SHOWDUINO_C3_FW_VERSION);

#if SHOWDUINO_WEBUI_ENABLED
  Serial1.begin(P4_UART_BAUD, SERIAL_8N1, P4_UART_RX_PIN, P4_UART_TX_PIN);
  p4WebTunnelSetPump(pumpP4Uart);

  webServerBegin(bootMs);
  gWebSocketManager.begin(SHOWDUINO_WEBSOCKET_PORT);
  deskUartBridgeBegin();
  webServerReassertAp();
#else
  Serial.println("WebUI disabled in BoardConfig.h");
#endif
}

void loop() {
#if SHOWDUINO_WEBUI_ENABLED
  pumpP4Uart();
  webServerLoop();
  gWebSocketManager.loop();
#endif
  delay(1);
}
