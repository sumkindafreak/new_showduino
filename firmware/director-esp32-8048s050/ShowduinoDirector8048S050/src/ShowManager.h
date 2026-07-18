#ifndef SHOWDUINO_SHOW_MANAGER_H
#define SHOWDUINO_SHOW_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "FileUtil.h"

#define SHOW_INDEX_MAX 32

struct ShowIndexEntry {
  char id[64];
  char name[64];
  char path[STORAGE_MAX_PATH_LEN];
};

class ShowManager {
public:
  bool loadIndex() {
    count = 0;
    dirty = false;
    if (!ShowduinoFileUtil::recoverAtomicFile(PATH_SHOW_INDEX)) {
      return rebuildIndex();
    }
    String json;
    if (!ShowduinoFileUtil::readTextFile(PATH_SHOW_INDEX, json)) return rebuildIndex();

    int arr = json.indexOf('[');
    int end = json.lastIndexOf(']');
    if (arr < 0 || end <= arr) return rebuildIndex();

    int pos = arr + 1;
    while (pos < end && count < SHOW_INDEX_MAX) {
      int o1 = json.indexOf('{', pos);
      if (o1 < 0 || o1 >= end) break;
      int o2 = json.indexOf('}', o1);
      if (o2 < 0) break;
      String obj = json.substring(o1, o2 + 1);
      ShowIndexEntry &e = entries[count];
      memset(&e, 0, sizeof(e));
      strncpy(e.id, ShowduinoFileUtil::jsonGetString(obj, "id", "").c_str(), sizeof(e.id) - 1);
      strncpy(e.name, ShowduinoFileUtil::jsonGetString(obj, "name", "").c_str(), sizeof(e.name) - 1);
      strncpy(e.path, ShowduinoFileUtil::jsonGetString(obj, "path", "").c_str(), sizeof(e.path) - 1);
      if (e.id[0]) count++;
      pos = o2 + 1;
    }

    ensureListFiles();
    ensureExampleShow();
    Serial.printf("[Shows] index loaded (%u shows)\n", count);
    return true;
  }

  bool rebuildIndex() {
    count = 0;
    File root = SD.open("/showduino/shows/packages");
    if (!root || !root.isDirectory()) {
      ShowduinoFileUtil::ensureDir("/showduino/shows/packages");
      root = SD.open("/showduino/shows/packages");
    }
    if (root && root.isDirectory()) {
      File ent = root.openNextFile();
      while (ent && count < SHOW_INDEX_MAX) {
        if (ent.isDirectory()) {
          String id = String(ent.name());
          int slash = id.lastIndexOf('/');
          if (slash >= 0) id = id.substring(slash + 1);
          String showJsonPath = String("/showduino/shows/packages/") + id + "/show.json";
          ShowIndexEntry &e = entries[count];
          memset(&e, 0, sizeof(e));
          strncpy(e.id, id.c_str(), sizeof(e.id) - 1);
          strncpy(e.path, showJsonPath.c_str(), sizeof(e.path) - 1);
          String body;
          if (ShowduinoFileUtil::readTextFile(showJsonPath.c_str(), body)) {
            strncpy(e.name, ShowduinoFileUtil::jsonGetString(body, "name", id.c_str()).c_str(), sizeof(e.name) - 1);
          } else {
            strncpy(e.name, id.c_str(), sizeof(e.name) - 1);
          }
          count++;
        }
        ent.close();
        ent = root.openNextFile();
      }
    }
    if (root) root.close();
    ensureExampleShow();
    return saveIndex();
  }

  bool saveIndex() {
    String json = "{\n  \"schemaVersion\": 1,\n  \"shows\": [\n";
    for (uint8_t i = 0; i < count; i++) {
      json += "    {\"id\": \"" + ShowduinoFileUtil::jsonEscape(entries[i].id) +
              "\", \"name\": \"" + ShowduinoFileUtil::jsonEscape(entries[i].name) +
              "\", \"path\": \"" + ShowduinoFileUtil::jsonEscape(entries[i].path) + "\"}";
      if (i + 1 < count) json += ",";
      json += "\n";
    }
    json += "  ]\n}\n";
    bool ok = ShowduinoFileUtil::atomicWriteTextFile(PATH_SHOW_INDEX, json);
    if (ok) dirty = false;
    return ok;
  }

  bool createShow(const ShowDefinition &show) {
    if (!show.id[0]) return false;
    String dir = String("/showduino/shows/packages/") + show.id;
    ShowduinoFileUtil::ensureDir(dir.c_str());
    ShowduinoFileUtil::ensureDir((dir + "/lighting").c_str());
    ShowduinoFileUtil::ensureDir((dir + "/audio").c_str());
    ShowduinoFileUtil::ensureDir((dir + "/video").c_str());
    ShowduinoFileUtil::ensureDir((dir + "/effects").c_str());
    ShowduinoFileUtil::ensureDir((dir + "/images").c_str());

    if (!writeShowFiles(dir, show)) return false;

    if (count < SHOW_INDEX_MAX) {
      strncpy(entries[count].id, show.id, sizeof(entries[count].id) - 1);
      strncpy(entries[count].name, show.name, sizeof(entries[count].name) - 1);
      String p = dir + "/show.json";
      strncpy(entries[count].path, p.c_str(), sizeof(entries[count].path) - 1);
      count++;
    }
    dirty = true;
    return saveIndex();
  }

  bool loadShow(const char *showId, ShowDefinition &out) {
    memset(&out, 0, sizeof(out));
    String path = String("/showduino/shows/packages/") + showId + "/show.json";
    String json;
    if (!ShowduinoFileUtil::readTextFile(path.c_str(), json)) return false;
    out.schemaVersion = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "schemaVersion", 1);
    strncpy(out.id, ShowduinoFileUtil::jsonGetString(json, "id", showId).c_str(), sizeof(out.id) - 1);
    strncpy(out.name, ShowduinoFileUtil::jsonGetString(json, "name", showId).c_str(), sizeof(out.name) - 1);
    strncpy(out.description, ShowduinoFileUtil::jsonGetString(json, "description", "").c_str(), sizeof(out.description) - 1);
    strncpy(out.author, ShowduinoFileUtil::jsonGetString(json, "author", "").c_str(), sizeof(out.author) - 1);
    strncpy(out.version, ShowduinoFileUtil::jsonGetString(json, "version", "1.0.0").c_str(), sizeof(out.version) - 1);
    out.durationSeconds = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "durationSeconds", 0);
    out.cueCount = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "cueCount", 0);
    strncpy(out.created, ShowduinoFileUtil::jsonGetString(json, "created", "").c_str(), sizeof(out.created) - 1);
    strncpy(out.modified, ShowduinoFileUtil::jsonGetString(json, "modified", "").c_str(), sizeof(out.modified) - 1);
    out.startCue = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "startCue", 1);
    out.stageControllerRequired = ShowduinoFileUtil::jsonGetBool(json, "stageControllerRequired", true);
    active = out;
    hasActive = true;
    return true;
  }

  bool saveShow(const ShowDefinition &show) {
    String dir = String("/showduino/shows/packages/") + show.id;
    if (!writeShowFiles(dir, show)) return false;
    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(entries[i].id, show.id) == 0) {
        strncpy(entries[i].name, show.name, sizeof(entries[i].name) - 1);
      }
    }
    dirty = true;
    showDirty = false;
    return saveIndex();
  }

  bool duplicateShow(const char *sourceId, const char *newId) {
    ShowDefinition src;
    if (!loadShow(sourceId, src)) return false;
    strncpy(src.id, newId, sizeof(src.id) - 1);
    String newName = String(src.name) + " Copy";
    strncpy(src.name, newName.c_str(), sizeof(src.name) - 1);
    return createShow(src);
  }

  bool renameShow(const char *showId, const char *newName) {
    ShowDefinition s;
    if (!loadShow(showId, s)) return false;
    strncpy(s.name, newName, sizeof(s.name) - 1);
    return saveShow(s);
  }

  bool deleteShow(const char *showId) {
    // Soft delete -> trash
    String src = String("/showduino/shows/packages/") + showId;
    String dst = String("/showduino/shows/trash/") + showId + "_" + String(millis());
    ShowduinoFileUtil::ensureDir("/showduino/shows/trash");
    // SD.rename only works for files on some FS — copy show.json marker then remove tree lightly.
    String marker = dst + "/show.json";
    String body;
    String srcJson = src + "/show.json";
    if (ShowduinoFileUtil::readTextFile(srcJson.c_str(), body)) {
      ShowduinoFileUtil::ensureDir(dst.c_str());
      ShowduinoFileUtil::atomicWriteTextFile(marker.c_str(), body);
    }
    SD.remove(srcJson.c_str());

    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(entries[i].id, showId) == 0) {
        for (uint8_t j = i; j + 1 < count; j++) entries[j] = entries[j + 1];
        count--;
        break;
      }
    }
    dirty = true;
    return saveIndex();
  }

  bool exportShow(const char *showId) {
    ShowDefinition s;
    if (!loadShow(showId, s)) return false;
    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/showduino/exports/shows/%s_export.json", showId);
    String body;
    String src = String("/showduino/shows/packages/") + showId + "/show.json";
    if (!ShowduinoFileUtil::readTextFile(src.c_str(), body)) return false;
    return ShowduinoFileUtil::atomicWriteTextFile(path, body);
  }

  bool importShow(const char *packagePath) {
    if (!ShowduinoFileUtil::pathLooksSafe(packagePath)) return false;
    String body;
    if (!ShowduinoFileUtil::readTextFile(packagePath, body)) return false;
    ShowDefinition s = {};
    s.schemaVersion = 1;
    strncpy(s.id, ShowduinoFileUtil::jsonGetString(body, "id", "imported").c_str(), sizeof(s.id) - 1);
    strncpy(s.name, ShowduinoFileUtil::jsonGetString(body, "name", s.id).c_str(), sizeof(s.name) - 1);
    return createShow(s);
  }

  void markDirty() { showDirty = true; dirty = true; }
  bool isDirty() const { return dirty || showDirty; }
  uint8_t size() const { return count; }
  const ShowIndexEntry *get(uint8_t i) const { return i < count ? &entries[i] : nullptr; }
  bool hasActiveShow() const { return hasActive; }
  const ShowDefinition &activeShow() const { return active; }

private:
  ShowIndexEntry entries[SHOW_INDEX_MAX];
  uint8_t count = 0;
  bool dirty = false;
  bool showDirty = false;
  bool hasActive = false;
  ShowDefinition active = {};

  void ensureListFiles() {
    if (!SD.exists(PATH_SHOW_FAVOURITES))
      ShowduinoFileUtil::atomicWriteTextFile(PATH_SHOW_FAVOURITES, "{\n  \"schemaVersion\": 1,\n  \"favourites\": []\n}\n");
    if (!SD.exists(PATH_SHOW_RECENT))
      ShowduinoFileUtil::atomicWriteTextFile(PATH_SHOW_RECENT, "{\n  \"schemaVersion\": 1,\n  \"recent\": []\n}\n");
  }

  void ensureExampleShow() {
    if (SD.exists("/showduino/shows/packages/example_show/show.json")) return;
    ShowDefinition ex = {};
    ex.schemaVersion = 1;
    strncpy(ex.id, "example_show", sizeof(ex.id));
    strncpy(ex.name, "Example Show", sizeof(ex.name));
    strncpy(ex.description, "Starter show package", sizeof(ex.description));
    strncpy(ex.author, "Showduino", sizeof(ex.author));
    strncpy(ex.version, "1.0.0", sizeof(ex.version));
    ex.durationSeconds = 120;
    ex.cueCount = 3;
    ex.startCue = 1;
    ex.stageControllerRequired = true;
    createShow(ex);
  }

  bool writeShowFiles(const String &dir, const ShowDefinition &show) {
    ShowduinoFileUtil::ensureDir(dir.c_str());
    String showJson = "{\n";
    showJson += "  \"schemaVersion\": " + String(show.schemaVersion ? show.schemaVersion : 1) + ",\n";
    showJson += "  \"id\": \"" + ShowduinoFileUtil::jsonEscape(show.id) + "\",\n";
    showJson += "  \"name\": \"" + ShowduinoFileUtil::jsonEscape(show.name) + "\",\n";
    showJson += "  \"description\": \"" + ShowduinoFileUtil::jsonEscape(show.description) + "\",\n";
    showJson += "  \"author\": \"" + ShowduinoFileUtil::jsonEscape(show.author) + "\",\n";
    showJson += "  \"version\": \"" + ShowduinoFileUtil::jsonEscape(show.version) + "\",\n";
    showJson += "  \"durationSeconds\": " + String(show.durationSeconds) + ",\n";
    showJson += "  \"cueCount\": " + String(show.cueCount) + ",\n";
    showJson += "  \"created\": \"" + ShowduinoFileUtil::jsonEscape(show.created) + "\",\n";
    showJson += "  \"modified\": \"" + ShowduinoFileUtil::jsonEscape(show.modified) + "\",\n";
    showJson += "  \"startCue\": " + String(show.startCue) + ",\n";
    showJson += "  \"stageControllerRequired\": " + String(show.stageControllerRequired ? "true" : "false") + "\n";
    showJson += "}\n";

    bool ok = ShowduinoFileUtil::atomicWriteTextFile((dir + "/show.json").c_str(), showJson);
    if (!SD.exists((dir + "/cues.json").c_str()))
      ShowduinoFileUtil::atomicWriteTextFile((dir + "/cues.json").c_str(), "{\n  \"schemaVersion\": 1,\n  \"cues\": []\n}\n");
    if (!SD.exists((dir + "/timeline.json").c_str()))
      ShowduinoFileUtil::atomicWriteTextFile((dir + "/timeline.json").c_str(), "{\n  \"schemaVersion\": 1,\n  \"events\": []\n}\n");
    if (!SD.exists((dir + "/devices.json").c_str()))
      ShowduinoFileUtil::atomicWriteTextFile((dir + "/devices.json").c_str(), "{\n  \"schemaVersion\": 1,\n  \"devices\": []\n}\n");
    if (!SD.exists((dir + "/media.json").c_str()))
      ShowduinoFileUtil::atomicWriteTextFile((dir + "/media.json").c_str(), "{\n  \"schemaVersion\": 1,\n  \"media\": []\n}\n");
    if (!SD.exists((dir + "/variables.json").c_str()))
      ShowduinoFileUtil::atomicWriteTextFile((dir + "/variables.json").c_str(), "{\n  \"schemaVersion\": 1,\n  \"variables\": {}\n}\n");
    return ok;
  }
};

#endif
