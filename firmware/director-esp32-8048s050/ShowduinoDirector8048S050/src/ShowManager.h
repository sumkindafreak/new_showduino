#ifndef SHOWDUINO_SHOW_MANAGER_H
#define SHOWDUINO_SHOW_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <ctype.h>
#include "StorageConfig.h"
#include "StorageTypes.h"
#include "FileUtil.h"

#define SHOW_INDEX_MAX 32
#define SHOW_PKG_ROOT  "/showduino/shows/packages"

struct ShowIndexEntry {
  char id[64];
  char name[64];
  char description[96];
  char author[48];
  char version[16];
  char path[STORAGE_MAX_PATH_LEN];   /* …/show.json */
  char folder[STORAGE_MAX_PATH_LEN]; /* package folder */
  uint32_t durationSeconds;
  bool hasThumbnail;
};

/**
 * Show package layout (future-safe; unknown files/dirs are ignored):
 *   /showduino/shows/packages/<id>/
 *     show.json          (required)
 *     thumbnail.bmp      (optional)
 *     audio/ lighting/ scripts/ timeline/ assets/ …
 */
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
      strncpy(e.name, ShowduinoFileUtil::jsonGetString(obj, "name", e.id).c_str(), sizeof(e.name) - 1);
      strncpy(e.path, ShowduinoFileUtil::jsonGetString(obj, "path", "").c_str(), sizeof(e.path) - 1);
      if (e.id[0] && e.path[0]) {
        enrichEntryFromDisk(e);
        count++;
      }
      pos = o2 + 1;
      yield();
    }

    ensureListFiles();
    ensureExampleShow();
    Serial.printf("[Shows] index loaded (%u shows)\n", count);
    return true;
  }

  /** Rescan SD packages — source of truth for the library UI. */
  bool rebuildIndex() {
    count = 0;
    ShowduinoFileUtil::ensureDir(SHOW_PKG_ROOT);
    File root = SD.open(SHOW_PKG_ROOT);
    if (!root || !root.isDirectory()) {
      if (root) root.close();
      ensureExampleShow();
      return saveIndex();
    }

    File ent = root.openNextFile();
    while (ent && count < SHOW_INDEX_MAX) {
      if (ent.isDirectory()) {
        char id[64] = {};
        extractLeafName(ent.name(), id, sizeof(id));
        if (id[0] && id[0] != '.') {
          char showJsonPath[STORAGE_MAX_PATH_LEN];
          snprintf(showJsonPath, sizeof(showJsonPath), "%s/%s/show.json", SHOW_PKG_ROOT, id);
          if (SD.exists(showJsonPath)) {
            ShowIndexEntry &e = entries[count];
            memset(&e, 0, sizeof(e));
            strncpy(e.id, id, sizeof(e.id) - 1);
            strncpy(e.path, showJsonPath, sizeof(e.path) - 1);
            snprintf(e.folder, sizeof(e.folder), "%s/%s", SHOW_PKG_ROOT, id);
            if (parseShowJsonIntoEntry(e)) {
              count++;
            }
          }
          /* Folders without show.json are ignored (audio/, trash leftovers, etc.). */
        }
      }
      /* Non-directory files under packages/ are ignored. */
      ent.close();
      ent = root.openNextFile();
      yield();
    }
    root.close();

    ensureExampleShow();
    bool ok = saveIndex();
    Serial.printf("[Shows] scanned %u packages from SD\n", count);
    return ok;
  }

  bool refreshLibrary() { return rebuildIndex(); }

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
    char dir[STORAGE_MAX_PATH_LEN];
    snprintf(dir, sizeof(dir), "%s/%s", SHOW_PKG_ROOT, show.id);
    ensurePackageDirs(dir);

    if (!writeShowFiles(dir, show)) return false;

    if (count < SHOW_INDEX_MAX) {
      ShowIndexEntry &e = entries[count];
      memset(&e, 0, sizeof(e));
      strncpy(e.id, show.id, sizeof(e.id) - 1);
      strncpy(e.name, show.name, sizeof(e.name) - 1);
      strncpy(e.description, show.description, sizeof(e.description) - 1);
      strncpy(e.author, show.author, sizeof(e.author) - 1);
      strncpy(e.version, show.version, sizeof(e.version) - 1);
      e.durationSeconds = show.durationSeconds;
      snprintf(e.folder, sizeof(e.folder), "%s", dir);
      snprintf(e.path, sizeof(e.path), "%s/show.json", dir);
      e.hasThumbnail = SD.exists((String(dir) + "/thumbnail.bmp").c_str());
      count++;
    }
    dirty = true;
    return saveIndex();
  }

  bool loadShow(const char *showId, ShowDefinition &out) {
    memset(&out, 0, sizeof(out));
    if (!showId || !showId[0]) return false;

    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s/show.json", SHOW_PKG_ROOT, showId);
    String json;
    if (!ShowduinoFileUtil::readTextFile(path, json)) return false;

    out.schemaVersion = (uint16_t)ShowduinoFileUtil::jsonGetLong(json, "schemaVersion", 1);
    strncpy(out.id, ShowduinoFileUtil::jsonGetString(json, "id", showId).c_str(), sizeof(out.id) - 1);
    String name = ShowduinoFileUtil::jsonGetString(json, "name", "");
    if (!name.length()) name = ShowduinoFileUtil::jsonGetString(json, "showName", showId);
    strncpy(out.name, name.c_str(), sizeof(out.name) - 1);
    strncpy(out.description, ShowduinoFileUtil::jsonGetString(json, "description", "").c_str(), sizeof(out.description) - 1);
    strncpy(out.author, ShowduinoFileUtil::jsonGetString(json, "author", "").c_str(), sizeof(out.author) - 1);
    strncpy(out.version, ShowduinoFileUtil::jsonGetString(json, "version", "1.0.0").c_str(), sizeof(out.version) - 1);
    out.durationSeconds = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "durationSeconds", 0);
    if (out.durationSeconds == 0) {
      out.durationSeconds = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "durationMs", 0) / 1000UL;
    }
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
    char dir[STORAGE_MAX_PATH_LEN];
    snprintf(dir, sizeof(dir), "%s/%s", SHOW_PKG_ROOT, show.id);
    if (!writeShowFiles(dir, show)) return false;
    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(entries[i].id, show.id) == 0) {
        strncpy(entries[i].name, show.name, sizeof(entries[i].name) - 1);
        strncpy(entries[i].description, show.description, sizeof(entries[i].description) - 1);
        strncpy(entries[i].author, show.author, sizeof(entries[i].author) - 1);
        strncpy(entries[i].version, show.version, sizeof(entries[i].version) - 1);
        entries[i].durationSeconds = show.durationSeconds;
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
    char src[STORAGE_MAX_PATH_LEN];
    char dst[STORAGE_MAX_PATH_LEN];
    snprintf(src, sizeof(src), "%s/%s", SHOW_PKG_ROOT, showId);
    snprintf(dst, sizeof(dst), "/showduino/shows/trash/%s_%lu", showId, (unsigned long)millis());
    ShowduinoFileUtil::ensureDir("/showduino/shows/trash");
    char marker[STORAGE_MAX_PATH_LEN];
    snprintf(marker, sizeof(marker), "%s/show.json", dst);
    String body;
    char srcJson[STORAGE_MAX_PATH_LEN];
    snprintf(srcJson, sizeof(srcJson), "%s/show.json", src);
    if (ShowduinoFileUtil::readTextFile(srcJson, body)) {
      ShowduinoFileUtil::ensureDir(dst);
      ShowduinoFileUtil::atomicWriteTextFile(marker, body);
    }
    SD.remove(srcJson);

    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(entries[i].id, showId) == 0) {
        for (uint8_t j = i; j + 1 < count; j++) entries[j] = entries[j + 1];
        count--;
        break;
      }
    }
    if (hasActive && strcmp(active.id, showId) == 0) {
      hasActive = false;
      memset(&active, 0, sizeof(active));
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
    char src[STORAGE_MAX_PATH_LEN];
    snprintf(src, sizeof(src), "%s/%s/show.json", SHOW_PKG_ROOT, showId);
    if (!ShowduinoFileUtil::readTextFile(src, body)) return false;
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

  bool thumbnailPath(const char *showId, char *out, size_t outLen) const {
    if (!showId || !out || outLen < 8) return false;
    snprintf(out, outLen, "%s/%s/thumbnail.bmp", SHOW_PKG_ROOT, showId);
    return SD.exists(out);
  }

  bool timelinePath(const char *showId, char *out, size_t outLen) const {
    if (!showId || !out || outLen < 8) return false;
    snprintf(out, outLen, "%s/%s/timeline.json", SHOW_PKG_ROOT, showId);
    return ShowduinoFileUtil::pathLooksSafe(out);
  }

  /** Resolve package id from id or display name (Show Management compatible). */
  const ShowIndexEntry *findByIdOrName(const char *key) const {
    if (!key || !key[0]) return nullptr;
    const ShowIndexEntry *byId = findById(key);
    if (byId) return byId;
    for (uint8_t i = 0; i < count; i++) {
      const char *a = entries[i].name;
      const char *b = key;
      bool match = true;
      while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
          match = false;
          break;
        }
        a++;
        b++;
      }
      if (match && *a == '\0' && *b == '\0') return &entries[i];
    }
    return nullptr;
  }

  const ShowIndexEntry *findById(const char *showId) const {
    if (!showId) return nullptr;
    for (uint8_t i = 0; i < count; i++) {
      if (strcmp(entries[i].id, showId) == 0) return &entries[i];
    }
    return nullptr;
  }

  void clearActive() {
    hasActive = false;
    memset(&active, 0, sizeof(active));
  }

  void markDirty() { showDirty = true; dirty = true; }
  bool isDirty() const { return dirty || showDirty; }
  uint8_t size() const { return count; }
  const ShowIndexEntry *get(uint8_t i) const { return i < count ? &entries[i] : nullptr; }
  bool hasActiveShow() const { return hasActive; }
  const ShowDefinition &activeShow() const { return active; }
  /** Alias for Stage 4 “currentShow” wording. */
  const ShowDefinition &currentShow() const { return active; }
  bool hasCurrentShow() const { return hasActive; }

private:
  ShowIndexEntry entries[SHOW_INDEX_MAX];
  uint8_t count = 0;
  bool dirty = false;
  bool showDirty = false;
  bool hasActive = false;
  ShowDefinition active = {};

  static void extractLeafName(const char *full, char *out, size_t outLen) {
    if (!out || outLen == 0) return;
    out[0] = '\0';
    if (!full) return;
    const char *slash = strrchr(full, '/');
    const char *leaf = slash ? slash + 1 : full;
    strncpy(out, leaf, outLen - 1);
    out[outLen - 1] = '\0';
  }

  static void ensurePackageDirs(const char *dir) {
    ShowduinoFileUtil::ensureDir(dir);
    char sub[STORAGE_MAX_PATH_LEN];
    static const char *kSubs[] = {
      "lighting", "audio", "video", "effects", "images",
      "scripts", "timeline", "assets", nullptr
    };
    for (uint8_t i = 0; kSubs[i]; i++) {
      snprintf(sub, sizeof(sub), "%s/%s", dir, kSubs[i]);
      ShowduinoFileUtil::ensureDir(sub);
    }
  }

  void enrichEntryFromDisk(ShowIndexEntry &e) {
    char folder[STORAGE_MAX_PATH_LEN];
    snprintf(folder, sizeof(folder), "%s/%s", SHOW_PKG_ROOT, e.id);
    strncpy(e.folder, folder, sizeof(e.folder) - 1);
    if (!e.path[0]) {
      snprintf(e.path, sizeof(e.path), "%s/show.json", folder);
    }
    parseShowJsonIntoEntry(e);
  }

  bool parseShowJsonIntoEntry(ShowIndexEntry &e) {
    String json;
    if (!ShowduinoFileUtil::readTextFile(e.path, json)) return false;

    String name = ShowduinoFileUtil::jsonGetString(json, "name", "");
    if (!name.length()) name = ShowduinoFileUtil::jsonGetString(json, "showName", e.id);
    strncpy(e.name, name.c_str(), sizeof(e.name) - 1);

    strncpy(e.description,
            ShowduinoFileUtil::jsonGetString(json, "description", "").c_str(),
            sizeof(e.description) - 1);
    strncpy(e.author,
            ShowduinoFileUtil::jsonGetString(json, "author", "").c_str(),
            sizeof(e.author) - 1);
    strncpy(e.version,
            ShowduinoFileUtil::jsonGetString(json, "version", "1.0.0").c_str(),
            sizeof(e.version) - 1);

    e.durationSeconds = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "durationSeconds", 0);
    if (e.durationSeconds == 0) {
      e.durationSeconds = (uint32_t)ShowduinoFileUtil::jsonGetLong(json, "durationMs", 0) / 1000UL;
    }

    char thumb[STORAGE_MAX_PATH_LEN];
    snprintf(thumb, sizeof(thumb), "%s/thumbnail.bmp", e.folder[0] ? e.folder : (SHOW_PKG_ROOT));
    if (!e.folder[0]) {
      snprintf(e.folder, sizeof(e.folder), "%s/%s", SHOW_PKG_ROOT, e.id);
      snprintf(thumb, sizeof(thumb), "%s/thumbnail.bmp", e.folder);
    }
    e.hasThumbnail = SD.exists(thumb);
    return e.name[0] != '\0';
  }

  void ensureListFiles() {
    if (!SD.exists(PATH_SHOW_FAVOURITES))
      ShowduinoFileUtil::atomicWriteTextFile(PATH_SHOW_FAVOURITES, "{\n  \"schemaVersion\": 1,\n  \"favourites\": []\n}\n");
    if (!SD.exists(PATH_SHOW_RECENT))
      ShowduinoFileUtil::atomicWriteTextFile(PATH_SHOW_RECENT, "{\n  \"schemaVersion\": 1,\n  \"recent\": []\n}\n");
  }

  void ensureExampleShow() {
    if (SD.exists(SHOW_PKG_ROOT "/example_show/show.json")) return;
    ShowDefinition ex = {};
    ex.schemaVersion = 1;
    strncpy(ex.id, "example_show", sizeof(ex.id));
    strncpy(ex.name, "Example Show", sizeof(ex.name));
    strncpy(ex.description, "Starter show package for Director library", sizeof(ex.description));
    strncpy(ex.author, "Showduino", sizeof(ex.author));
    strncpy(ex.version, "1.0.0", sizeof(ex.version));
    ex.durationSeconds = 120;
    ex.cueCount = 3;
    ex.startCue = 1;
    ex.stageControllerRequired = true;
    createShow(ex);
  }

  bool writeShowFiles(const char *dir, const ShowDefinition &show) {
    ensurePackageDirs(dir);
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

    char path[STORAGE_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/show.json", dir);
    bool ok = ShowduinoFileUtil::atomicWriteTextFile(path, showJson);

    auto ensureJson = [&](const char *name, const char *body) {
      snprintf(path, sizeof(path), "%s/%s", dir, name);
      if (!SD.exists(path)) ShowduinoFileUtil::atomicWriteTextFile(path, body);
    };
    ensureJson("cues.json", "{\n  \"schemaVersion\": 1,\n  \"cues\": []\n}\n");
    ensureJson("timeline.json",
               "{\n  \"schemaVersion\": 1,\n  \"timeline\": [\n"
               "    {\"time\": 0, \"command\": \"STATUS:REQUEST\"}\n"
               "  ]\n}\n");
    ensureJson("devices.json", "{\n  \"schemaVersion\": 1,\n  \"devices\": []\n}\n");
    ensureJson("media.json", "{\n  \"schemaVersion\": 1,\n  \"media\": []\n}\n");
    ensureJson("variables.json", "{\n  \"schemaVersion\": 1,\n  \"variables\": {}\n}\n");
    return ok;
  }
};
#endif
