#include "ShowHttp.h"

#include <WebServer.h>
#include "../WebApiHandler.h"
#include "ShowNetwork.h"

static WebServer *sServer = nullptr;
static bool sListening = false;
static bool sUrlLogged = false;

static void httpReply(int status, const char *mime, const char *body, size_t len) {
  if (!sServer) return;
  sServer->sendHeader("Access-Control-Allow-Origin", "*");
  sServer->sendHeader("Cache-Control", "no-store");
  if (mime && mime[0]) {
    sServer->send((status > 0) ? status : 200, mime, body ? body : "");
  } else {
    sServer->send((status > 0) ? status : 200, "text/plain", body ? body : "");
  }
  (void)len;
}

static void handleApi() {
  const String method = sServer->method() == HTTP_POST ? "POST" : "GET";
  String path = sServer->uri();
  if (sServer->args() > 0 && path.indexOf('?') < 0) {
    path += '?';
    for (int i = 0; i < sServer->args(); i++) {
      if (i) path += '&';
      path += sServer->argName(i);
      path += '=';
      path += sServer->arg(i);
    }
  }
  String body;
  if (method == "POST") body = sServer->arg("plain");
  webApiSetReplySink(httpReply);
  if (!webApiDispatch(method.c_str(), path.c_str(), body.c_str())) {
    sServer->sendHeader("Access-Control-Allow-Origin", "*");
    sServer->send(404, "application/json", "{\"error\":\"not_found\"}\n");
  }
  webApiSetReplySink(nullptr);
}

static void handleRoot() {
  static const char kPage[] =
      "<!doctype html><html><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Showduino P4</title>"
      "<style>body{font-family:Segoe UI,sans-serif;background:#0a0a0a;color:#e8e8e8;"
      "margin:0;padding:1.2rem}h1{color:#c41e1e;font-size:1.2rem}"
      ".card{background:#141414;border:1px solid #2a2a2a;border-radius:8px;"
      "padding:1rem;margin:0.8rem 0}label{display:block;color:#888;font-size:.8rem}"
      "pre{white-space:pre-wrap}</style></head><body>"
      "<h1>Showduino Show Engine</h1>"
      "<p>Ethernet reachability page. Full Studio WebUI stays on the Comms S3 SoftAP. "
      "This is the same P4 API origin, not a second product.</p>"
      "<div class=card><h2>Show Network</h2><pre id=net>loading…</pre></div>"
      "<div class=card><h2>E1.31 Test Receiver</h2><pre id=e131>loading…</pre></div>"
      "<script>"
      "async function j(p){const r=await fetch(p);return r.json();}"
      "function paint(){"
      "j('/api/network').then(d=>{"
      "const e=d.ethernet||{};const s=d.saved&&d.saved.ethernet||{};"
      "net.textContent="
      "'LIVE\\nlink '+(e.link||'?')+'\\nmode '+(e.mode||'?')+"
      "'\\nIP '+(e.ip||'--')+'\\nMAC '+(e.mac||'--')+"
      "'\\n\\nSAVED\\nenabled '+(s.enabled?'YES':'NO')+'\\nmode '+(s.mode||'?');"
      "}).catch(()=>net.textContent='API unavailable');"
      "j('/api/e131').then(d=>{"
      "e131.textContent='Status '+(d.state||'?')+'\\nUniverse '+(d.universe||'?')+"
      "'\\nSource '+(d.source||'--')+'\\nPriority '+(d.priority||0)+"
      "'\\nRate '+(d.rateFps||0)+' fps\\nPackets '+(d.packets||0)+"
      "'\\nRejected '+(d.rejected||0);"
      "}).catch(()=>e131.textContent='API unavailable');"
      "}"
      "paint();setInterval(paint,2000);"
      "</script></body></html>";
  sServer->sendHeader("Cache-Control", "no-store");
  sServer->send(200, "text/html", kPage);
}

static void handleCors() {
  sServer->sendHeader("Access-Control-Allow-Origin", "*");
  sServer->sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  sServer->sendHeader("Access-Control-Allow-Headers", "Content-Type");
  sServer->send(204);
}

void showHttpBegin() {
  sListening = false;
  sUrlLogged = false;
}

void showHttpOnAddress(const char *ip) {
  if (!sServer) {
    sServer = new WebServer(80);
    sServer->on("/", HTTP_GET, handleRoot);
    sServer->on("/api/network", HTTP_GET, handleApi);
    sServer->on("/api/e131", HTTP_GET, handleApi);
    sServer->on("/api/e131/channels", HTTP_GET, handleApi);
    sServer->on("/api/system", HTTP_GET, handleApi);
    sServer->on("/api/time", HTTP_GET, handleApi);
    sServer->on("/api/show", HTTP_GET, handleApi);
    sServer->on("/api/command", HTTP_POST, handleApi);
    sServer->on("/api/network", HTTP_OPTIONS, handleCors);
    sServer->on("/api/e131", HTTP_OPTIONS, handleCors);
    sServer->on("/api/command", HTTP_OPTIONS, handleCors);
    sServer->onNotFound(handleApi);
    sServer->begin();
  }
  sListening = true;
  if (ip && ip[0] && !sUrlLogged) {
    Serial.printf("[WEB] http://%s/\n", ip);
    sUrlLogged = true;
  }
}

void showHttpOnLinkLost() {
  sListening = false;
  sUrlLogged = false;
}

void showHttpLoop() {
  if (sServer && sListening) sServer->handleClient();
}

bool showHttpListening() {
  return sListening && sServer != nullptr;
}
