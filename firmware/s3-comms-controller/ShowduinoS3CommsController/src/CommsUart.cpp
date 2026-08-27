#include "CommsUart.h"
#include "../BoardConfig.h"

#include <string.h>

static bool sReady = false;
static bool sEverRx = false;
static uint32_t sLastRxMs = 0;
static uint32_t sRxCount = 0;
static uint32_t sTxCount = 0;
static uint32_t sDropped = 0;
static String sBuf;

void commsUartBegin() {
  /* Leave TX idle as input until the UART driver takes it, so an unpowered
   * P4 on GPIO17 cannot clamp the pin as hard during the first microseconds. */
  pinMode(SHOWDUINO_COMMS_UART_TX_PIN, INPUT);
  pinMode(SHOWDUINO_COMMS_UART_RX_PIN, INPUT);
  delay(20);
  Serial1.setRxBufferSize(SHOWDUINO_COMMS_UART_RX_BUFFER);
  Serial1.begin(SHOWDUINO_COMMS_UART_BAUD, SHOWDUINO_COMMS_UART_CONFIG,
                SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN);
  delay(20);
  sReady = true;
  sBuf = "";
}

bool commsUartReady() {
  return sReady;
}

void commsUartWriteLine(const char *line) {
  if (!sReady || !line || !line[0]) return;
  Serial1.println(line);
  Serial1.flush();
  sTxCount++;
}

bool commsUartReadLine(char *out, size_t outSize) {
  if (!sReady || !out || outSize < 2) return false;

  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (sBuf.length() == 0) continue;
      sBuf.trim();
      if (sBuf.length() == 0) continue;
      if (sBuf.length() >= outSize) {
        sDropped++;
        sBuf = "";
        return false;
      }
      bool bad = false;
      for (unsigned i = 0; i < sBuf.length(); i++) {
        unsigned char ch = (unsigned char)sBuf[i];
        if (ch < 32 || ch > 126) {
          bad = true;
          break;
        }
      }
      if (bad) {
        sDropped++;
        sBuf = "";
        return false;
      }
      strncpy(out, sBuf.c_str(), outSize - 1);
      out[outSize - 1] = '\0';
      sBuf = "";
      sLastRxMs = millis();
      sEverRx = true;
      sRxCount++;
      return true;
    }

    sBuf += c;
    if (sBuf.length() > SHOWDUINO_COMMS_LINE_MAX) {
      sDropped++;
      sBuf = "";
    }
  }
  return false;
}

uint32_t commsUartLastRxMs() { return sLastRxMs; }
uint32_t commsUartRxCount() { return sRxCount; }
uint32_t commsUartTxCount() { return sTxCount; }
uint32_t commsUartDroppedCount() { return sDropped; }
bool commsUartEverRx() { return sEverRx; }
