#ifndef SHOWDUINO_ASSET_MANAGER_H
#define SHOWDUINO_ASSET_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "StorageConfig.h"
#include "FileUtil.h"

struct AssetCacheSlot {
  char key[32];
  char path[STORAGE_MAX_PATH_LEN];
  bool valid;
  unsigned long lastUsedMs;
};

class AssetManager {
public:
  bool loadManifest() {
    ensureDefaultManifest();
    String json;
    if (!ShowduinoFileUtil::readTextFile(PATH_ASSET_MANIFEST, json)) {
      Serial.println("[Assets] manifest missing");
      return false;
    }
    manifest = json;
    Serial.println("[Assets] UI asset manifest loaded");
    return true;
  }

  bool resolveBackground(const char *name, char *outPath, size_t outLen) {
    return resolveFromSection("backgrounds", name, outPath, outLen,
                              "/showduino/ui/backgrounds/");
  }

  bool resolveIcon(const char *name, char *outPath, size_t outLen) {
    return resolveFromSection("icons", name, outPath, outLen,
                              "/showduino/ui/icons/");
  }

  bool exists(const char *path) {
    if (!ShowduinoFileUtil::pathLooksSafe(path)) return false;
    return SD.exists(path);
  }

  // Path cache only — decoding stays in the UI/GFX layer.
  const char *cachePath(const char *key, const char *path) {
    if (!key || !path) return path;
    int slot = findSlot(key);
    if (slot < 0) slot = evictSlot();
    strncpy(cache[slot].key, key, sizeof(cache[slot].key) - 1);
    strncpy(cache[slot].path, path, sizeof(cache[slot].path) - 1);
    cache[slot].valid = true;
    cache[slot].lastUsedMs = millis();
    return cache[slot].path;
  }

  bool getCached(const char *key, char *outPath, size_t outLen) {
    int slot = findSlot(key);
    if (slot < 0) return false;
    cache[slot].lastUsedMs = millis();
    strncpy(outPath, cache[slot].path, outLen - 1);
    outPath[outLen - 1] = '\0';
    return true;
  }

private:
  String manifest;
  AssetCacheSlot cache[STORAGE_ASSET_CACHE_SLOTS] = {};

  void ensureDefaultManifest() {
    if (SD.exists(PATH_ASSET_MANIFEST)) return;
    String m = "{\n  \"schemaVersion\": 1,\n";
    m += "  \"backgrounds\": {\n";
    m += "    \"boot\": \"/showduino/ui/backgrounds/boot.jpg\",\n";
    m += "    \"home\": \"/showduino/ui/backgrounds/home.jpg\",\n";
    m += "    \"shows\": \"/showduino/ui/backgrounds/shows.jpg\",\n";
    m += "    \"devices\": \"/showduino/ui/backgrounds/devices.jpg\",\n";
    m += "    \"timeline\": \"/showduino/ui/backgrounds/timeline.jpg\",\n";
    m += "    \"show_control\": \"/showduino/ui/backgrounds/show_control.jpg\",\n";
    m += "    \"manual\": \"/showduino/ui/backgrounds/manual.jpg\",\n";
    m += "    \"diagnostics\": \"/showduino/ui/backgrounds/diagnostics.jpg\",\n";
    m += "    \"settings\": \"/showduino/ui/backgrounds/settings.jpg\",\n";
    m += "    \"connected\": \"/showduino/ui/backgrounds/connected.jpg\",\n";
    m += "    \"disconnected\": \"/showduino/ui/backgrounds/disconnected.jpg\",\n";
    m += "    \"warning\": \"/showduino/ui/backgrounds/warning.jpg\",\n";
    m += "    \"emergency\": \"/showduino/ui/backgrounds/emergency.jpg\"\n";
    m += "  },\n";
    m += "  \"icons\": {\n";
    m += "    \"play48\": \"/showduino/ui/icons/transport/play_48.png\",\n";
    m += "    \"pause48\": \"/showduino/ui/icons/transport/pause_48.png\",\n";
    m += "    \"stop48\": \"/showduino/ui/icons/transport/stop_48.png\",\n";
    m += "    \"online24\": \"/showduino/ui/icons/status/online_24.png\"\n";
    m += "  }\n}\n";
    ShowduinoFileUtil::atomicWriteTextFile(PATH_ASSET_MANIFEST, m);
  }

  bool resolveFromSection(const char *section, const char *name, char *outPath, size_t outLen,
                          const char *fallbackPrefix) {
    if (!name || !outPath || outLen == 0) return false;

    // Try manifest string lookup: "name": "/path"
    String key = String("\"") + name + "\"";
    int k = manifest.indexOf(key);
    if (k >= 0) {
      String path = ShowduinoFileUtil::jsonGetString(manifest, name, "");
      if (path.length() > 0) {
        strncpy(outPath, path.c_str(), outLen - 1);
        outPath[outLen - 1] = '\0';
        if (exists(outPath)) return true;
      }
    }

    // Fallback conventional path
    snprintf(outPath, outLen, "%s%s.jpg", fallbackPrefix, name);
    if (exists(outPath)) return true;
    snprintf(outPath, outLen, "%s%s.png", fallbackPrefix, name);
    return exists(outPath);
  }

  int findSlot(const char *key) {
    for (uint8_t i = 0; i < STORAGE_ASSET_CACHE_SLOTS; i++) {
      if (cache[i].valid && strcmp(cache[i].key, key) == 0) return i;
    }
    return -1;
  }

  int evictSlot() {
    int best = 0;
    unsigned long oldest = ULONG_MAX;
    for (uint8_t i = 0; i < STORAGE_ASSET_CACHE_SLOTS; i++) {
      if (!cache[i].valid) return i;
      if (cache[i].lastUsedMs < oldest) {
        oldest = cache[i].lastUsedMs;
        best = i;
      }
    }
    return best;
  }
};

#endif
