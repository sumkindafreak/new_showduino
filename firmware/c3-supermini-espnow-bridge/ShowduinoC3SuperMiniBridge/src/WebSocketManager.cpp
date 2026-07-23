#include "WebSocketManager.h"
#include "DeviceManager.h"
#include <WiFi.h>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

WebSocketManager gWebSocketManager;

void WebSocketManager::begin(uint16_t port) {
  if (server_) return;
  server_ = new WiFiServer(port);
  server_->begin();
  ready_ = true;
  Serial.printf("[WebSocket] listening on :%u\n", (unsigned)port);
}

String WebSocketManager::makeAcceptKey(const String &clientKey) {
  String src = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char sha[20];
  mbedtls_sha1((const unsigned char *)src.c_str(), src.length(), sha);
  unsigned char b64[40];
  size_t olen = 0;
  mbedtls_base64_encode(b64, sizeof(b64), &olen, sha, 20);
  b64[olen] = 0;
  return String((char *)b64);
}

bool WebSocketManager::doHandshake(WiFiClient &client, const String &req) {
  int keyIdx = req.indexOf("Sec-WebSocket-Key:");
  if (keyIdx < 0) return false;
  int lineEnd = req.indexOf('\n', keyIdx);
  if (lineEnd < 0) lineEnd = req.length();
  String keyLine = req.substring(keyIdx + 18, lineEnd);
  keyLine.trim();
  if (keyLine.endsWith("\r")) keyLine.remove(keyLine.length() - 1);
  String accept = makeAcceptKey(keyLine);

  String resp;
  resp.reserve(180);
  resp += "HTTP/1.1 101 Switching Protocols\r\n";
  resp += "Upgrade: websocket\r\n";
  resp += "Connection: Upgrade\r\n";
  resp += "Sec-WebSocket-Accept: ";
  resp += accept;
  resp += "\r\n\r\n";
  client.print(resp);
  return true;
}

void WebSocketManager::sendFrame(WiFiClient &client, const String &payload) {
  if (!client.connected()) return;
  const size_t len = payload.length();
  uint8_t hdr[4];
  size_t hdrLen = 2;
  hdr[0] = 0x81; /* FIN + text */
  if (len < 126) {
    hdr[1] = (uint8_t)len;
  } else if (len <= 0xFFFF) {
    hdr[1] = 126;
    hdr[2] = (uint8_t)((len >> 8) & 0xFF);
    hdr[3] = (uint8_t)(len & 0xFF);
    hdrLen = 4;
  } else {
    return;
  }
  client.write(hdr, hdrLen);
  client.write((const uint8_t *)payload.c_str(), len);
}

void WebSocketManager::broadcastText(const String &message) {
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (!clients_[i].open) continue;
    if (!clients_[i].sock.connected()) {
      clients_[i].sock.stop();
      clients_[i].open = false;
      continue;
    }
    sendFrame(clients_[i].sock, message);
  }
}

void WebSocketManager::sendDeviceEvent(const char *eventName, const DeviceRecord &device) {
  String body = "{\"event\":\"";
  body += eventName ? eventName : "device.updated";
  body += "\",\"device\":";
  DeviceManager::appendOneDeviceJson(device, body);
  body += '}';
  broadcastText(body);
}

void WebSocketManager::sendNetworkStats(const NetworkStatistics &stats) {
  String body = "{\"event\":\"network.stats\",\"stats\":{";
  body += "\"deviceCount\":"; body += String(stats.deviceCount); body += ',';
  body += "\"onlineCount\":"; body += String(stats.onlineCount); body += ',';
  body += "\"warningCount\":"; body += String(stats.warningCount); body += ',';
  body += "\"offlineCount\":"; body += String(stats.offlineCount); body += ',';
  body += "\"averageRssi\":"; body += String(stats.averageRssi); body += ',';
  body += "\"heartbeatRate\":"; body += String(stats.heartbeatRatePerMin); body += ',';
  body += "\"networkHealth\":\""; body += stats.health; body += "\"";
  body += "}}";
  broadcastText(body);
}

void WebSocketManager::sendSnapshot(const String &devicesJson, const String &networkJson) {
  String body = "{\"event\":\"snapshot\",\"devices\":";
  int arr = devicesJson.indexOf('[');
  int arrEnd = devicesJson.lastIndexOf(']');
  if (arr >= 0 && arrEnd > arr) body += devicesJson.substring(arr, arrEnd + 1);
  else body += "[]";
  body += ",\"network\":";
  body += networkJson;
  body += '}';
  broadcastText(body);
}

void WebSocketManager::sendCommandEvent(const char *eventName, const String &commandJson) {
  String body = "{\"event\":\"";
  body += eventName ? eventName : "command.updated";
  body += "\"";
  if (commandJson.length() > 0) {
    body += ",\"command\":";
    body += commandJson;
  }
  body += '}';
  broadcastText(body);
}

void WebSocketManager::sendQueueUpdated(size_t depth, size_t emergencyDepth) {
  String body = "{\"event\":\"queue.updated\",\"queueDepth\":";
  body += String((unsigned)depth);
  body += ",\"emergencyDepth\":";
  body += String((unsigned)emergencyDepth);
  body += '}';
  broadcastText(body);
}


void WebSocketManager::sendJsonEvent(const char *eventName, const char *detailJson) {
  String body = "{\"event\":\"";
  body += eventName ? eventName : "event";
  body += "\"";
  if (detailJson && detailJson[0]) {
    body += ",\"data\":";
    body += detailJson;
  }
  body += '}';
  broadcastText(body);
}
void WebSocketManager::acceptNew() {
  if (!server_) return;
  WiFiClient incoming = server_->available();
  if (!incoming) return;
  if (!incoming.connected()) return;

  String req;
  req.reserve(512);
  uint32_t start = millis();
  while (millis() - start < 800) {
    while (incoming.available()) {
      char c = (char)incoming.read();
      req += c;
      if (req.indexOf("\r\n\r\n") >= 0) break;
    }
    if (req.indexOf("\r\n\r\n") >= 0) break;
    delay(1);
  }

  size_t slot = MAX_CLIENTS;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (!clients_[i].open) { slot = i; break; }
  }
  if (slot >= MAX_CLIENTS) {
    incoming.stop();
    return;
  }
  if (!doHandshake(incoming, req)) {
    incoming.stop();
    return;
  }
  clients_[slot].sock = incoming;
  clients_[slot].open = true;
  Serial.println("[WebSocket] client connected");

  String devices, network;
  gDeviceManager.appendDevicesJson(devices);
  gDeviceManager.appendNetworkJson(network, millis());
  String snap;
  snap.reserve(devices.length() + network.length() + 64);
  snap += "{\"event\":\"snapshot\",\"devices\":";
  int arr = devices.indexOf('[');
  int arrEnd = devices.lastIndexOf(']');
  if (arr >= 0 && arrEnd > arr) snap += devices.substring(arr, arrEnd + 1);
  else snap += "[]";
  snap += ",\"network\":";
  snap += network;
  snap += '}';
  sendFrame(clients_[slot].sock, snap);
}

void WebSocketManager::serviceClient(size_t idx) {
  Client &c = clients_[idx];
  if (!c.open) return;
  if (!c.sock.connected()) {
    c.sock.stop();
    c.open = false;
    Serial.println("[WebSocket] client disconnected");
    return;
  }
  while (c.sock.available() >= 2) {
    uint8_t b0 = (uint8_t)c.sock.read();
    uint8_t b1 = (uint8_t)c.sock.read();
    uint8_t opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    size_t len = (size_t)(b1 & 0x7F);
    if (len == 126) {
      if (c.sock.available() < 2) return;
      len = ((size_t)c.sock.read() << 8) | (size_t)c.sock.read();
    } else if (len == 127) {
      /* discard oversized */
      for (int i = 0; i < 8 && c.sock.available(); i++) (void)c.sock.read();
      len = 0;
    }
    uint8_t mask[4] = {0};
    if (masked) {
      for (int i = 0; i < 4; i++) mask[i] = c.sock.available() ? (uint8_t)c.sock.read() : 0;
    }
    for (size_t i = 0; i < len; i++) {
      if (!c.sock.available()) break;
      (void)c.sock.read(); /* ignore client payload for Stage 5 */
      (void)mask;
    }
    if (opcode == 0x8) { /* close */
      c.sock.stop();
      c.open = false;
      return;
    }
    if (opcode == 0x9) { /* ping -> pong */
      uint8_t pong[2] = {0x8A, 0x00};
      c.sock.write(pong, 2);
    }
  }
}

void WebSocketManager::loop() {
  if (!ready_ || !server_) return;
  acceptNew();
  for (size_t i = 0; i < MAX_CLIENTS; i++) serviceClient(i);
}