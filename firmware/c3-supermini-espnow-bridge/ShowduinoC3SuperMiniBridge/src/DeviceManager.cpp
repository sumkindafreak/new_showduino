#include "DeviceManager.h"
#include "../BoardConfig.h"
#include "capability/CapabilityManager.h"
#include <string.h>

DeviceManager gDeviceManager;

void DeviceManager::begin(HeartbeatManager *hb, DeviceEventLog *log) {
  hb_ = hb;
  log_ = log;
  memset(devices_, 0, sizeof(devices_));
  lastEvalMs_ = millis();
  lastPulseWindowMs_ = lastEvalMs_;
  ready_ = true;
}

size_t DeviceManager::count() const {
  size_t n = 0;
  for (size_t i = 0; i < MAX_DEVICES; i++) if (devices_[i].inUse) n++;
  return n;
}

DeviceRecord *DeviceManager::findByMac(const uint8_t mac[6]) {
  if (!mac) return nullptr;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices_[i].inUse) continue;
    if (memcmp(devices_[i].mac, mac, 6) == 0) return &devices_[i];
  }
  return nullptr;
}

DeviceRecord *DeviceManager::findById(const char *id) {
  if (!id || !id[0]) return nullptr;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (devices_[i].inUse && strcmp(devices_[i].id, id) == 0) return &devices_[i];
  }
  return nullptr;
}

const DeviceRecord *DeviceManager::getById(const char *id) const {
  if (!id || !id[0]) return nullptr;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (devices_[i].inUse && strcmp(devices_[i].id, id) == 0) return &devices_[i];
  }
  return nullptr;
}

const DeviceRecord *DeviceManager::getByIndex(size_t index) const {
  size_t n = 0;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices_[i].inUse) continue;
    if (n == index) return &devices_[i];
    n++;
  }
  return nullptr;
}

DeviceRecord *DeviceManager::allocSlot() {
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices_[i].inUse) {
      memset(&devices_[i], 0, sizeof(DeviceRecord));
      devices_[i].inUse = true;
      devices_[i].routePriority = 50;
      devices_[i].preferred = false;
      devices_[i].availability = DeviceAvailability::Unavailable;
      strncpy(devices_[i].batteryStatus, "n/a", sizeof(devices_[i].batteryStatus) - 1);
      return &devices_[i];
    }
  }
  return nullptr;
}

void DeviceManager::applyBoardDefaults(DeviceRecord &d) {
  if (!d.boardType[0]) return;
  if (strcmp(d.boardType, "sue") == 0) {
    if (!d.capabilities[0]) {
      strncpy(d.capabilities, "NetworkBridge,Logging,OTA,MediaStorage", sizeof(d.capabilities) - 1);
    }
    d.routePriority = 80;
    d.preferred = false;
  } else if (strcmp(d.boardType, "ian") == 0) {
    if (!d.capabilities[0]) {
      strncpy(d.capabilities,
              "SceneRuntime,Scheduler,RelayOutput,Lighting,AudioPlayback,PixelOutput,Temperature,Humidity,Logging",
              sizeof(d.capabilities) - 1);
    }
    d.routePriority = 90;
    d.preferred = true;
  } else if (strcmp(d.boardType, "director") == 0) {
    if (!d.capabilities[0]) {
      strncpy(d.capabilities, "Touchscreen,OLED,Logging", sizeof(d.capabilities) - 1);
    }
    d.routePriority = 40;
    d.preferred = false;
  } else if (strcmp(d.boardType, "relay") == 0) {
    if (!d.capabilities[0]) {
      strncpy(d.capabilities, "RelayOutput,GPIOInput", sizeof(d.capabilities) - 1);
    }
    d.routePriority = 70;
    d.preferred = true;
  }
}

void DeviceManager::syncAvailability(DeviceRecord &d) {
  if (d.presence == DevicePresence::Offline) {
    d.availability = DeviceAvailability::Unavailable;
  } else if (d.availability != DeviceAvailability::Busy) {
    d.availability = DeviceAvailability::Available;
  }
}

void DeviceManager::emit(const char *eventName, const DeviceRecord &d) {
  if (log_) {
    char detail[72];
    snprintf(detail, sizeof(detail), "%s (%s)", d.friendlyName[0] ? d.friendlyName : d.id, d.boardType);
    log_->log(eventName, detail);
  }
  if (changeFn_) changeFn_(eventName, d);
}

void DeviceManager::touch(DeviceRecord &d, uint32_t nowMs, int8_t rssi, bool hasRssi) {
  const DevicePresence prev = d.presence;
  const int8_t prevRssi = d.rssi;
  d.lastSeenMs = nowMs;
  if (hasRssi) d.rssi = rssi;
  if (hb_) d.presence = hb_->evaluate(d.lastSeenMs, nowMs);
  else d.presence = DevicePresence::Online;
  syncAvailability(d);

  noteHeartbeatPulse();

  if (prev != d.presence) {
    if (d.presence == DevicePresence::Online) emit("device.online", d);
    else if (d.presence == DevicePresence::Offline) emit("device.offline", d);
    else emit("device.heartbeat", d);
  } else if (hasRssi && abs((int)prevRssi - (int)rssi) >= 3) {
    emit("device.rssi", d);
  }
}

void DeviceManager::registerLocal(const char *boardType, const char *friendlyName, const uint8_t mac[6],
                                  const char *fw, const char *proto, const char *caps, const char *ip) {
  if (!ready_ || !mac) return;
  DeviceRecord *d = findByMac(mac);
  bool discovered = false;
  char prevCaps[128] = {0};
  if (!d) {
    d = allocSlot();
    if (!d) return;
    discovered = true;
    deviceIdFromMac(boardType, mac, d->id, sizeof(d->id));
    memcpy(d->mac, mac, 6);
    d->discoveredMs = millis();
  } else {
    strncpy(prevCaps, d->capabilities, sizeof(prevCaps) - 1);
  }
  strncpy(d->boardType, boardType ? boardType : "node", sizeof(d->boardType) - 1);
  strncpy(d->friendlyName, friendlyName ? friendlyName : boardType, sizeof(d->friendlyName) - 1);
  strncpy(d->firmwareVersion, fw ? fw : "", sizeof(d->firmwareVersion) - 1);
  strncpy(d->protocolVersion, proto ? proto : "", sizeof(d->protocolVersion) - 1);
  if (caps && caps[0]) strncpy(d->capabilities, caps, sizeof(d->capabilities) - 1);
  applyBoardDefaults(*d);
  strncpy(d->connectionType, "local", sizeof(d->connectionType) - 1);
  if (ip && ip[0]) strncpy(d->ip, ip, sizeof(d->ip) - 1);
  d->wifiConnected = true;
  d->espNowActive = true;
  touch(*d, millis(), 0, false);
  if (discovered) {
    emit("device.discovered", *d);
    if (gCapabilityManager.ready()) { /* may not be ready yet at boot */ }
    gCapabilityManager.notifyCapabilityChanged(*d, "capability.registered");
  } else {
    emit("device.updated", *d);
    if (strcmp(prevCaps, d->capabilities) != 0) {
      gCapabilityManager.notifyCapabilityChanged(*d, "capability.changed");
    }
  }
}

void DeviceManager::noteEspNowSighting(const char *boardType, const char *friendlyName, const uint8_t mac[6],
                                       int8_t rssi, const char *caps) {
  if (!ready_ || !mac) return;
  DeviceRecord *d = findByMac(mac);
  bool discovered = false;
  if (!d) {
    d = allocSlot();
    if (!d) return;
    discovered = true;
    deviceIdFromMac(boardType, mac, d->id, sizeof(d->id));
    memcpy(d->mac, mac, 6);
    d->discoveredMs = millis();
    strncpy(d->boardType, boardType ? boardType : "node", sizeof(d->boardType) - 1);
    strncpy(d->friendlyName, friendlyName ? friendlyName : boardType, sizeof(d->friendlyName) - 1);
    snprintf(d->protocolVersion, sizeof(d->protocolVersion), "%d.%d",
             SHOWDUINO_PROTOCOL_VERSION_MAJOR, SHOWDUINO_PROTOCOL_VERSION_MINOR);
    strncpy(d->firmwareVersion, "unknown", sizeof(d->firmwareVersion) - 1);
    if (caps && caps[0]) strncpy(d->capabilities, caps, sizeof(d->capabilities) - 1);
    applyBoardDefaults(*d);
    strncpy(d->connectionType, "esp-now", sizeof(d->connectionType) - 1);
    d->espNowActive = true;
  } else {
    if (!d->friendlyName[0] && friendlyName) {
      strncpy(d->friendlyName, friendlyName, sizeof(d->friendlyName) - 1);
    }
    d->espNowActive = true;
    strncpy(d->connectionType, "esp-now", sizeof(d->connectionType) - 1);
  }
  touch(*d, millis(), rssi, true);
  if (discovered) {
    emit("device.discovered", *d);
    gCapabilityManager.notifyCapabilityChanged(*d, "capability.registered");
  }
}

void DeviceManager::noteUartSighting(const char *boardType, const char *friendlyName, const char *idHint,
                                     const char *caps) {
  if (!ready_) return;
  DeviceRecord *d = nullptr;
  if (idHint && idHint[0]) d = findById(idHint);
  if (!d) {
    for (size_t i = 0; i < MAX_DEVICES; i++) {
      if (devices_[i].inUse && strcmp(devices_[i].boardType, boardType ? boardType : "") == 0) {
        d = &devices_[i];
        break;
      }
    }
  }
  bool discovered = false;
  if (!d) {
    d = allocSlot();
    if (!d) return;
    discovered = true;
    if (idHint && idHint[0]) strncpy(d->id, idHint, sizeof(d->id) - 1);
    else snprintf(d->id, sizeof(d->id), "%s-uart", boardType ? boardType : "node");
    d->discoveredMs = millis();
    strncpy(d->boardType, boardType ? boardType : "node", sizeof(d->boardType) - 1);
    strncpy(d->friendlyName, friendlyName ? friendlyName : boardType, sizeof(d->friendlyName) - 1);
    snprintf(d->protocolVersion, sizeof(d->protocolVersion), "%d.%d",
             SHOWDUINO_PROTOCOL_VERSION_MAJOR, SHOWDUINO_PROTOCOL_VERSION_MINOR);
    strncpy(d->firmwareVersion, "stage-link", sizeof(d->firmwareVersion) - 1);
    if (caps && caps[0]) strncpy(d->capabilities, caps, sizeof(d->capabilities) - 1);
    applyBoardDefaults(*d);
    strncpy(d->connectionType, "uart", sizeof(d->connectionType) - 1);
  }
  touch(*d, millis(), 0, false);
  if (discovered) {
    emit("device.discovered", *d);
    gCapabilityManager.notifyCapabilityChanged(*d, "capability.registered");
  }
}

bool DeviceManager::renameDevice(const char *id, const char *friendlyName) {
  if (!ready_) return false;
  DeviceRecord *d = findById(id);
  if (!d || !friendlyName || !friendlyName[0]) return false;
  if (strcmp(d->friendlyName, friendlyName) == 0) return true;
  strncpy(d->friendlyName, friendlyName, sizeof(d->friendlyName) - 1);
  d->friendlyName[sizeof(d->friendlyName) - 1] = '\0';
  emit("device.renamed", *d);
  return true;
}

bool DeviceManager::setDeviceCapabilities(const char *id, const char *caps) {
  if (!ready_ || !id || !caps) return false;
  DeviceRecord *d = findById(id);
  if (!d) return false;
  if (strcmp(d->capabilities, caps) == 0) return true;
  strncpy(d->capabilities, caps, sizeof(d->capabilities) - 1);
  d->capabilities[sizeof(d->capabilities) - 1] = '\0';
  emit("device.updated", *d);
  gCapabilityManager.notifyCapabilityChanged(*d, "capability.changed");
  return true;
}

bool DeviceManager::setDevicePreferred(const char *id, bool preferred, uint8_t priority) {
  if (!ready_ || !id) return false;
  DeviceRecord *d = findById(id);
  if (!d) return false;
  d->preferred = preferred;
  d->routePriority = priority;
  emit("device.updated", *d);
  return true;
}

void DeviceManager::loop(uint32_t nowMs) {
  if (!ready_) return;
  if (nowMs - lastPulseWindowMs_ >= 60000UL) {
    ratePerMin_ = pulsesInWindow_;
    pulsesInWindow_ = 0;
    lastPulseWindowMs_ = nowMs;
  }
  if (nowMs - lastEvalMs_ < 500UL) return;
  lastEvalMs_ = nowMs;
  if (!hb_) return;

  for (size_t i = 0; i < MAX_DEVICES; i++) {
    DeviceRecord &d = devices_[i];
    if (!d.inUse) continue;
    DevicePresence next = hb_->evaluate(d.lastSeenMs, nowMs);
    if (next == d.presence) continue;
    DevicePresence prev = d.presence;
    d.presence = next;
    syncAvailability(d);
    if (next == DevicePresence::Offline && prev != DevicePresence::Offline) {
      d.espNowActive = (strcmp(d.connectionType, "esp-now") == 0) ? false : d.espNowActive;
      emit("device.offline", d);
      emit("device.heartbeat_timeout", d);
    } else if (next == DevicePresence::Online && prev != DevicePresence::Online) {
      emit("device.online", d);
      emit("connection.restored", d);
    } else {
      emit("device.heartbeat", d);
    }
  }
}

void DeviceManager::noteHeartbeatPulse() {
  heartbeatPulses_++;
  pulsesInWindow_++;
}

void DeviceManager::computeNetworkStats(NetworkStatistics &out, uint32_t nowMs) const {
  out = NetworkStatistics();
  out.computedMs = nowMs;
  int32_t rssiSum = 0;
  uint16_t rssiN = 0;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices_[i].inUse) continue;
    out.deviceCount++;
    switch (devices_[i].presence) {
      case DevicePresence::Online: out.onlineCount++; break;
      case DevicePresence::Warning: out.warningCount++; break;
      default: out.offlineCount++; break;
    }
    if (devices_[i].presence != DevicePresence::Offline &&
        strcmp(devices_[i].connectionType, "esp-now") == 0) {
      rssiSum += devices_[i].rssi;
      rssiN++;
    }
  }
  out.averageRssi = rssiN ? (int16_t)(rssiSum / (int32_t)rssiN) : 0;
  out.heartbeatRatePerMin = ratePerMin_;
  if (out.deviceCount == 0) strncpy(out.health, "empty", sizeof(out.health) - 1);
  else if (out.offlineCount == 0 && out.warningCount == 0) strncpy(out.health, "healthy", sizeof(out.health) - 1);
  else if (out.onlineCount == 0) strncpy(out.health, "down", sizeof(out.health) - 1);
  else if (out.warningCount > 0 || out.offlineCount > 0) strncpy(out.health, "degraded", sizeof(out.health) - 1);
  else strncpy(out.health, "ok", sizeof(out.health) - 1);
}

void DeviceManager::appendOneDeviceJson(const DeviceRecord &d, String &out) {
  char macStr[18];
  deviceMacToString(d.mac, macStr, sizeof(macStr));
  out += '{';
  out += "\"id\":\""; out += d.id; out += "\",";
  out += "\"boardType\":\""; out += d.boardType; out += "\",";
  out += "\"board\":\""; out += d.boardType; out += "\",";
  out += "\"name\":\""; out += d.friendlyName; out += "\",";
  out += "\"friendlyName\":\""; out += d.friendlyName; out += "\",";
  out += "\"mac\":\""; out += macStr; out += "\",";
  out += "\"firmware\":\""; out += d.firmwareVersion; out += "\",";
  out += "\"firmwareVersion\":\""; out += d.firmwareVersion; out += "\",";
  out += "\"protocol\":\""; out += d.protocolVersion; out += "\",";
  out += "\"protocolVersion\":\""; out += d.protocolVersion; out += "\",";
  out += "\"online\":"; out += (d.presence == DevicePresence::Online) ? "true" : "false"; out += ',';
  out += "\"presence\":\""; out += devicePresenceName(d.presence); out += "\",";
  out += "\"availability\":\""; out += deviceAvailabilityName(d.availability); out += "\",";
  out += "\"priority\":"; out += String((unsigned)d.routePriority); out += ',';
  out += "\"preferred\":"; out += d.preferred ? "true" : "false"; out += ',';
  out += "\"lastSeenMs\":"; out += String(d.lastSeenMs); out += ',';
  out += "\"discoveredMs\":"; out += String(d.discoveredMs); out += ',';
  out += "\"rssi\":"; out += String((int)d.rssi); out += ',';
  out += "\"ip\":\""; out += d.ip; out += "\",";
  out += "\"wifiConnected\":"; out += d.wifiConnected ? "true" : "false"; out += ',';
  out += "\"wifiStatus\":\""; out += d.wifiConnected ? "connected" : "n/a"; out += "\",";
  out += "\"espNowActive\":"; out += d.espNowActive ? "true" : "false"; out += ',';
  out += "\"espNowStatus\":\""; out += d.espNowActive ? "active" : "idle"; out += "\",";
  out += "\"capabilities\":\""; out += d.capabilities; out += "\",";
  out += "\"batteryStatus\":\""; out += d.batteryStatus; out += "\",";
  out += "\"connectionType\":\""; out += d.connectionType; out += "\",";
  out += "\"connectionStatus\":\""; out += devicePresenceName(d.presence); out += "\",";
  out += "\"role\":\""; out += d.boardType; out += "\"";
  out += '}';
}

void DeviceManager::appendDevicesJson(String &out) const {
  out += "{\"devices\":[";
  bool first = true;
  for (size_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices_[i].inUse) continue;
    if (!first) out += ',';
    first = false;
    appendOneDeviceJson(devices_[i], out);
  }
  out += "]}";
}

bool DeviceManager::appendDeviceJsonById(const char *id, String &out) const {
  const DeviceRecord *d = getById(id);
  if (!d) return false;
  appendOneDeviceJson(*d, out);
  return true;
}

void DeviceManager::appendNetworkJson(String &out, uint32_t nowMs) const {
  NetworkStatistics st;
  computeNetworkStats(st, nowMs);
  out += '{';
  out += "\"deviceCount\":"; out += String(st.deviceCount); out += ',';
  out += "\"onlineCount\":"; out += String(st.onlineCount); out += ',';
  out += "\"warningCount\":"; out += String(st.warningCount); out += ',';
  out += "\"offlineCount\":"; out += String(st.offlineCount); out += ',';
  out += "\"averageRssi\":"; out += String(st.averageRssi); out += ',';
  out += "\"heartbeatRate\":"; out += String(st.heartbeatRatePerMin); out += ',';
  out += "\"networkHealth\":\""; out += st.health; out += "\",";
  out += "\"health\":\""; out += st.health; out += "\",";
  out += "\"computedMs\":"; out += String(st.computedMs);
  if (log_) {
    out += ",\"recentEvents\":";
    String ev;
    log_->appendJsonArray(ev);
    out += ev;
  }
  out += '}';
}