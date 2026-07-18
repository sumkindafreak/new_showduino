#ifndef SHOWDUINO_DEVICE_DATABASE_H
#define SHOWDUINO_DEVICE_DATABASE_H

#include <Arduino.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "FileUtil.h"

#define DEVICE_DB_MAX 24

class DeviceDatabase {
public:
  bool load() {
    count = 0;
    dirty = false;
    if (!ShowduinoFileUtil::recoverAtomicFile(PATH_PAIRED_DEVICES)) {
      return save();  // create empty DB
    }
    String json;
    if (!ShowduinoFileUtil::readTextFile(PATH_PAIRED_DEVICES, json)) return false;

    // Minimal array parser for objects inside "devices":[ {...}, {...} ]
    int arr = json.indexOf('[');
    int end = json.lastIndexOf(']');
    if (arr < 0 || end <= arr) {
      Serial.println("[Devices] empty or missing devices array");
      return true;
    }

    int pos = arr + 1;
    while (pos < end && count < DEVICE_DB_MAX) {
      int objStart = json.indexOf('{', pos);
      if (objStart < 0 || objStart >= end) break;
      int objEnd = json.indexOf('}', objStart);
      if (objEnd < 0) break;
      String obj = json.substring(objStart, objEnd + 1);
      DeviceRecord &d = devices[count];
      memset(&d, 0, sizeof(d));
      strncpy(d.id, ShowduinoFileUtil::jsonGetString(obj, "id", "").c_str(), sizeof(d.id) - 1);
      strncpy(d.name, ShowduinoFileUtil::jsonGetString(obj, "name", "").c_str(), sizeof(d.name) - 1);
      strncpy(d.type, ShowduinoFileUtil::jsonGetString(obj, "type", "node").c_str(), sizeof(d.type) - 1);
      strncpy(d.mac, ShowduinoFileUtil::jsonGetString(obj, "mac", "").c_str(), sizeof(d.mac) - 1);
      d.protocolVersion = (uint16_t)ShowduinoFileUtil::jsonGetLong(obj, "protocolVersion", 1);
      d.paired = ShowduinoFileUtil::jsonGetBool(obj, "paired", true);
      d.trusted = ShowduinoFileUtil::jsonGetBool(obj, "trusted", true);
      strncpy(d.lastSeen, ShowduinoFileUtil::jsonGetString(obj, "lastSeen", "").c_str(), sizeof(d.lastSeen) - 1);
      strncpy(d.firmware, ShowduinoFileUtil::jsonGetString(obj, "firmware", "").c_str(), sizeof(d.firmware) - 1);
      strncpy(d.capabilities, ShowduinoFileUtil::jsonGetString(obj, "capabilities", "").c_str(), sizeof(d.capabilities) - 1);
      if (d.id[0] || d.mac[0]) count++;
      pos = objEnd + 1;
    }

    if (!SD.exists(PATH_STAGE_CONTROLLERS)) {
      ShowduinoFileUtil::atomicWriteTextFile(PATH_STAGE_CONTROLLERS, "{\n  \"schemaVersion\": 1,\n  \"controllers\": []\n}\n");
    }
    if (!SD.exists(PATH_NODES_JSON)) {
      ShowduinoFileUtil::atomicWriteTextFile(PATH_NODES_JSON, "{\n  \"schemaVersion\": 1,\n  \"nodes\": []\n}\n");
    }

    Serial.printf("[Devices] loaded %u paired records\n", count);
    return true;
  }

  bool save() {
    String json = "{\n  \"schemaVersion\": 1,\n  \"devices\": [\n";
    for (uint8_t i = 0; i < count; i++) {
      const DeviceRecord &d = devices[i];
      json += "    {\n";
      json += "      \"id\": \"" + ShowduinoFileUtil::jsonEscape(d.id) + "\",\n";
      json += "      \"name\": \"" + ShowduinoFileUtil::jsonEscape(d.name) + "\",\n";
      json += "      \"type\": \"" + ShowduinoFileUtil::jsonEscape(d.type) + "\",\n";
      json += "      \"mac\": \"" + ShowduinoFileUtil::jsonEscape(d.mac) + "\",\n";
      json += "      \"protocolVersion\": " + String(d.protocolVersion) + ",\n";
      json += "      \"paired\": " + String(d.paired ? "true" : "false") + ",\n";
      json += "      \"trusted\": " + String(d.trusted ? "true" : "false") + ",\n";
      json += "      \"lastSeen\": \"" + ShowduinoFileUtil::jsonEscape(d.lastSeen) + "\",\n";
      json += "      \"firmware\": \"" + ShowduinoFileUtil::jsonEscape(d.firmware) + "\",\n";
      json += "      \"capabilities\": \"" + ShowduinoFileUtil::jsonEscape(d.capabilities) + "\"\n";
      json += "    }";
      if (i + 1 < count) json += ",";
      json += "\n";
    }
    json += "  ]\n}\n";

    bool ok = ShowduinoFileUtil::atomicWriteTextFile(PATH_PAIRED_DEVICES, json);
    if (ok) {
      dirty = false;
      lastSavedMs = millis();
      Serial.println("[Devices] paired_devices.json saved");
    }
    return ok;
  }

  bool upsert(const DeviceRecord &rec, bool immediateSave) {
    int idx = findById(rec.id);
    if (idx < 0) idx = findByMac(rec.mac);
    if (idx >= 0) {
      devices[idx] = rec;
    } else if (count < DEVICE_DB_MAX) {
      devices[count++] = rec;
    } else {
      return false;
    }
    dirty = true;
    if (immediateSave) return save();
    return true;
  }

  bool removeById(const char *id, bool immediateSave = true) {
    int idx = findById(id);
    if (idx < 0) return false;
    for (uint8_t i = idx; i + 1 < count; i++) devices[i] = devices[i + 1];
    count--;
    dirty = true;
    if (immediateSave) return save();
    return true;
  }

  void touchLastSeen(const char *idOrMac, const char *stamp) {
    int idx = findById(idOrMac);
    if (idx < 0) idx = findByMac(idOrMac);
    if (idx < 0) return;
    strncpy(devices[idx].lastSeen, stamp, sizeof(devices[idx].lastSeen) - 1);
    // RAM only — periodic snapshot saves to SD.
    seenDirty = true;
  }

  void processPeriodicSave(unsigned long intervalMs = 60000UL) {
    if ((dirty || seenDirty) && (millis() - lastSavedMs >= intervalMs)) {
      save();
      seenDirty = false;
    }
  }

  void markDirty() { dirty = true; }
  bool isDirty() const { return dirty || seenDirty; }
  uint8_t size() const { return count; }
  const DeviceRecord *get(uint8_t i) const { return i < count ? &devices[i] : nullptr; }

private:
  DeviceRecord devices[DEVICE_DB_MAX];
  uint8_t count = 0;
  bool dirty = false;
  bool seenDirty = false;
  unsigned long lastSavedMs = 0;

  int findById(const char *id) {
    if (!id || !id[0]) return -1;
    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(devices[i].id, id) == 0) return i;
    }
    return -1;
  }

  int findByMac(const char *mac) {
    if (!mac || !mac[0]) return -1;
    for (uint8_t i = 0; i < count; i++) {
      if (strcasecmp(devices[i].mac, mac) == 0) return i;
    }
    return -1;
  }
};

#endif
