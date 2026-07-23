#include "P4WebTunnel.h"
#include "../BoardConfig.h"

#if SHOWDUINO_WEBUI_ENABLED
#include "../../../protocol/showduino_web_tunnel.h"

enum TunnelRxState : uint8_t {
  TUNNEL_IDLE = 0,
  TUNNEL_AWAIT_BODY
};

static TunnelRxState sRxState = TUNNEL_IDLE;
static String sProxyBody;
static int sProxyStatus = 0;
static size_t sBodyExpected = 0;
static size_t sBodyReceived = 0;
static bool sProxyReady = false;
static bool sProxyWaiting = false;
static P4LinkPumpFn sPumpFn = nullptr;

void p4WebTunnelSetPump(P4LinkPumpFn fn) {
  sPumpFn = fn;
}

static void resetProxyWait() {
  sProxyWaiting = false;
  sProxyReady = false;
  sProxyBody = "";
  sProxyStatus = 0;
  sRxState = TUNNEL_IDLE;
  sBodyExpected = 0;
  sBodyReceived = 0;
}

static void finishProxyBody() {
  sRxState = TUNNEL_IDLE;
  sProxyReady = true;
}

static void parseWebrHeader(const String &line) {
  if (!line.startsWith(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) return;
  const int base = (int)strlen(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX);
  int colon = line.indexOf(':', base);
  if (colon < 0) return;

  sProxyStatus = line.substring(base, colon).toInt();
  sBodyExpected = (size_t)line.substring(colon + 1).toInt();
  if (sBodyExpected > SHOWDUINO_WEB_TUNNEL_BODY_MAX) {
    sBodyExpected = SHOWDUINO_WEB_TUNNEL_BODY_MAX;
  }
  sProxyBody = "";
  sBodyReceived = 0;
  if (sBodyExpected == 0) {
    finishProxyBody();
    return;
  }
  sRxState = TUNNEL_AWAIT_BODY;
}

void p4WebTunnelBegin() {
  resetProxyWait();
}

bool p4WebTunnelConsumingBytes() {
  return sRxState == TUNNEL_AWAIT_BODY;
}

void p4WebTunnelOnByte(char c) {
  if (sRxState != TUNNEL_AWAIT_BODY) return;
  sProxyBody += c;
  sBodyReceived++;
  if (sBodyReceived >= sBodyExpected) {
    finishProxyBody();
  }
}

bool p4WebTunnelOnLine(const String &line) {
  if (!sProxyWaiting) return false;
  if (!line.startsWith(SHOWDUINO_WEB_TUNNEL_RESP_PREFIX)) return false;
  parseWebrHeader(line);
  return true;
}

bool p4WebTunnelGet(const char *path, String &bodyOut, int &statusOut, uint32_t timeoutMs) {
  if (!path) return false;

  resetProxyWait();
  sProxyWaiting = true;

  /* Drain any stale line fragments so we sync on the next WEBR header. */
  if (sPumpFn) sPumpFn();

  Serial1.print(SHOWDUINO_WEB_TUNNEL_REQ_PREFIX);
  Serial1.print("GET");
  Serial1.print(path);
  Serial1.print('\n');
  Serial1.flush();

  Serial.printf("[WebUI] UART → P4 WEB/GET%s (timeout %lums)\n", path, (unsigned long)timeoutMs);

  const uint32_t deadline = millis() + timeoutMs;
  while (millis() < deadline) {
    if (sPumpFn) sPumpFn();
    if (sProxyReady) {
      bodyOut = sProxyBody;
      statusOut = sProxyStatus;
      resetProxyWait();
      return true;
    }
    delay(1);
    yield();
  }

  Serial.println("[WebUI] UART ← P4 timeout (no WEBR:)");
  resetProxyWait();
  return false;
}

#else

void p4WebTunnelBegin() {}
bool p4WebTunnelConsumingBytes() { return false; }
void p4WebTunnelOnByte(char c) { (void)c; }
bool p4WebTunnelOnLine(const String &line) { (void)line; return false; }

bool p4WebTunnelGet(const char *path, String &bodyOut, int &statusOut, uint32_t timeoutMs) {
  (void)path;
  (void)bodyOut;
  (void)statusOut;
  (void)timeoutMs;
  return false;
}

#endif
