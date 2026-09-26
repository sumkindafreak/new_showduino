#include "CommsWebTunnel.h"
#include "../CommsUart.h"
#include "../../BoardConfig.h"
#include "../../../protocol/showduino_deploy.h"
#include "../../../protocol/showduino_web_tunnel.h"

#include <string.h>

enum TunnelRxState : uint8_t {
  TUNNEL_IDLE = 0,
  TUNNEL_AWAIT_BODY
};

static TunnelRxState sRxState = TUNNEL_IDLE;
static String sProxyBody;
static String sProxyMime;
static int sProxyStatus = 0;
static size_t sBodyExpected = 0;
static size_t sBodyReceived = 0;
static bool sProxyReady = false;
static bool sProxyWaiting = false;
static CommsWebPumpFn sPumpFn = nullptr;

void commsWebTunnelSetPump(CommsWebPumpFn fn) {
  sPumpFn = fn;
}

static void resetProxyWait() {
  sProxyWaiting = false;
  sProxyReady = false;
  sProxyBody = "";
  sProxyMime = "";
  sProxyStatus = 0;
  sRxState = TUNNEL_IDLE;
  sBodyExpected = 0;
  sBodyReceived = 0;
}

static void finishProxyBody() {
  sRxState = TUNNEL_IDLE;
  sProxyReady = true;
}

/* After timeout/malformed frames, discard residual body bytes so the
 * line-oriented desk parser can recover. Yields so emergency/ESP-NOW service
 * continues during drain (worst-case full BODY_MAX ~2 s @ 115200). */
static void drainStaleTunnelBytes(uint32_t quietMs, uint32_t maxMs) {
  const uint32_t start = millis();
  uint32_t lastByte = start;
  while ((int32_t)(millis() - start) < (int32_t)maxMs) {
    int drained = 0;
    while (commsUartAvailable() > 0) {
      (void)commsUartRead();
      drained++;
      lastByte = millis();
      if ((drained & 0x3F) == 0) {
        if (sPumpFn) sPumpFn();
        yield();
      }
    }
    if ((int32_t)(millis() - lastByte) >= (int32_t)quietMs) break;
    if (sPumpFn) sPumpFn();
    delay(1);
    yield();
  }
  commsUartClearLineBuffer();
}

static bool parseWebrHeader(const char *line) {
  ShowduinoWebrHeader hdr;
  if (!showduino_webr_parse_header(line, &hdr)) return false;

  sProxyMime = hdr.mime;
  sProxyStatus = hdr.status;
  sBodyExpected = hdr.bodyLen;
  sProxyBody = "";
  sProxyBody.reserve((unsigned)sBodyExpected + 1);
  sBodyReceived = 0;
  if (sBodyExpected == 0) {
    finishProxyBody();
    return true;
  }
  sRxState = TUNNEL_AWAIT_BODY;
  return true;
}

void commsWebTunnelBegin() {
  resetProxyWait();
}

bool commsWebTunnelConsumingBytes() {
  return sRxState == TUNNEL_AWAIT_BODY;
}

bool commsWebTunnelWaiting() {
  return sProxyWaiting;
}

void commsWebTunnelOnByte(char c) {
  if (sRxState != TUNNEL_AWAIT_BODY) return;
  sProxyBody += c;
  sBodyReceived++;
  if (sBodyReceived >= sBodyExpected) finishProxyBody();
}

bool commsWebTunnelOnLine(const char *line) {
  if (!sProxyWaiting || !line) return false;
  if (strncmp(line, SHOWDUINO_WEB_TUNNEL_RESP_PREFIX,
              strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) != 0) {
    return false;
  }
  if (!parseWebrHeader(line)) {
    /* Malformed WEBR while waiting — recover; surface as transport error. */
    Serial.println("[WEBUI] Malformed WEBR header — draining UART");
    drainStaleTunnelBytes(40, 500);
    sProxyStatus = 502;
    sProxyBody = "{\"ok\":false,\"error\":\"webr_malformed\"}\n";
    sProxyMime = "application/json";
    sProxyReady = true;
    sRxState = TUNNEL_IDLE;
    sProxyWaiting = true; /* still complete the waiter */
    return true;
  }
  return true;
}

static bool waitForProxy(String &bodyOut, int &statusOut, String &mimeOut,
                         uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  uint32_t lastYield = millis();
  while ((int32_t)(millis() - deadline) < 0) {
    if (sPumpFn) sPumpFn();
    if (sProxyReady) {
      bodyOut = sProxyBody;
      statusOut = sProxyStatus;
      mimeOut = sProxyMime;
      resetProxyWait();
      return true;
    }
    /* Chunked scheduling: yield often so ESP-NOW / emergency keep running
     * during long WEBR bodies (~2 s for BODY_MAX at 115200). */
    if ((millis() - lastYield) >= 5UL) {
      lastYield = millis();
      yield();
    } else {
      delay(1);
    }
  }

  Serial.println("[WEBUI] UART <- P4 timeout (no complete WEBR)");
  if (sRxState == TUNNEL_AWAIT_BODY) {
    drainStaleTunnelBytes(50, 2500);
  } else {
    drainStaleTunnelBytes(30, 200);
  }
  resetProxyWait();
  return false;
}

bool commsWebTunnelGet(const char *path, String &bodyOut, int &statusOut,
                       String &mimeOut, uint32_t timeoutMs) {
  if (!path || !commsUartReady()) return false;

  resetProxyWait();
  sProxyWaiting = true;
  if (sPumpFn) sPumpFn();

  char req[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(req, sizeof(req), "%sGET%s", SHOWDUINO_WEB_TUNNEL_REQ_PREFIX, path);
  commsUartWriteLine(req);
  return waitForProxy(bodyOut, statusOut, mimeOut, timeoutMs);
}

bool commsWebTunnelPost(const char *path, String &bodyOut, int &statusOut,
                        String &mimeOut, uint32_t timeoutMs) {
  if (!path || !commsUartReady()) return false;

  resetProxyWait();
  sProxyWaiting = true;
  if (sPumpFn) sPumpFn();

  char req[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(req, sizeof(req), "%sPOST%s", SHOWDUINO_WEB_TUNNEL_REQ_PREFIX, path);
  commsUartWriteLine(req);
  return waitForProxy(bodyOut, statusOut, mimeOut, timeoutMs);
}

bool commsWebTunnelPostBody(const char *method, const char *path,
                            const uint8_t *data, size_t len,
                            String &bodyOut, int &statusOut, String &mimeOut,
                            uint32_t timeoutMs) {
  if (!method || !path || !commsUartReady()) return false;
  if (len > SHOWDUINO_DEPLOY_CHUNK_MAX) return false;

  resetProxyWait();
  sProxyWaiting = true;
  if (sPumpFn) sPumpFn();

  char req[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(req, sizeof(req), "%s%u:%s:%s",
           SHOWDUINO_WEB_BODY_REQ_PREFIX, (unsigned)len, method, path);
  /* Atomic header+body so desk STATUS/DIAG cannot interleave mid-frame. */
  commsUartWriteLineAndBytes(req, data, len);
  return waitForProxy(bodyOut, statusOut, mimeOut, timeoutMs);
}
