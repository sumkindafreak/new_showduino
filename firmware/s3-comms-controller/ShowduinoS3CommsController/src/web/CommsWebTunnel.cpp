#include "CommsWebTunnel.h"
#include "../CommsUart.h"
#include "../../BoardConfig.h"

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

static bool parseWebrHeader(const char *line) {
  if (!line || strncmp(line, SHOWDUINO_WEB_TUNNEL_RESP_PREFIX,
                       strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) != 0) {
    return false;
  }

  /* WEBR:<status>:<bodyLen>[:<mime>] */
  const char *p = line + strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  int status = 0;
  while (*p >= '0' && *p <= '9') {
    status = status * 10 + (*p - '0');
    p++;
  }
  if (*p != ':') return false;
  p++;
  size_t bodyLen = 0;
  while (*p >= '0' && *p <= '9') {
    bodyLen = bodyLen * 10 + (size_t)(*p - '0');
    p++;
  }

  sProxyMime = "";
  if (*p == ':') {
    sProxyMime = String(p + 1);
    sProxyMime.trim();
  }

  sProxyStatus = status;
  if (bodyLen > SHOWDUINO_WEB_TUNNEL_BODY_MAX) bodyLen = SHOWDUINO_WEB_TUNNEL_BODY_MAX;
  sBodyExpected = bodyLen;
  sProxyBody = "";
  sProxyBody.reserve((unsigned)bodyLen + 1);
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
  return parseWebrHeader(line);
}

bool commsWebTunnelGet(const char *path, String &bodyOut, int &statusOut,
                       String &mimeOut, uint32_t timeoutMs) {
  if (!path || !commsUartReady()) return false;

  resetProxyWait();
  sProxyWaiting = true;

  if (sPumpFn) sPumpFn();

  /* Existing P4 parser: WEB/GET/<path> */
  char req[SHOWDUINO_COMMS_LINE_MAX + 1];
  snprintf(req, sizeof(req), "%sGET%s", SHOWDUINO_WEB_TUNNEL_REQ_PREFIX, path);
  commsUartWriteLine(req);

  const uint32_t deadline = millis() + timeoutMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (sPumpFn) sPumpFn();
    if (sProxyReady) {
      bodyOut = sProxyBody;
      statusOut = sProxyStatus;
      mimeOut = sProxyMime;
      resetProxyWait();
      return true;
    }
    delay(1);
    yield();
  }

  Serial.println("[WEBUI] UART <- P4 timeout (no WEBR:)");
  resetProxyWait();
  return false;
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

  const uint32_t deadline = millis() + timeoutMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (sPumpFn) sPumpFn();
    if (sProxyReady) {
      bodyOut = sProxyBody;
      statusOut = sProxyStatus;
      mimeOut = sProxyMime;
      resetProxyWait();
      return true;
    }
    delay(1);
    yield();
  }

  Serial.println("[WEBUI] UART <- P4 timeout (no WEBR:)");
  resetProxyWait();
  return false;
}
