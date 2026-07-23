#ifndef SHOWDUINO_STORAGE_CONFIG_H
#define SHOWDUINO_STORAGE_CONFIG_H

#include <Arduino.h>
#include "../BoardConfig.h"

// =========================================================
// Showduino Director — SD storage paths & limits
// Panel is 800x480 (ST7262); asset docs may say 800x400.
// =========================================================

#define STORAGE_ROOT                "/showduino"
#define STORAGE_SPI_HZ              10000000
#define STORAGE_SCHEMA_VERSION      1
#define STORAGE_FW_VERSION          "0.9.0-director"

#define STORAGE_AUTOSAVE_DEFAULT_MS 15000UL
#define STORAGE_AUTOSAVE_MIN_MS     5000UL
#define STORAGE_AUTOSAVE_MAX_MS     30000UL
#define STORAGE_MOUNT_RETRY_MS      15000UL
#define STORAGE_SHUTDOWN_TIMEOUT_MS 8000UL
#define STORAGE_PACKET_DEBUG_MS     120000UL

#define STORAGE_MAX_PATH_LEN        180
#define STORAGE_MAX_NAME_LEN        64
#define STORAGE_MAX_JSON_BYTES      49152
#define STORAGE_MAX_LOG_LINE        320
#define STORAGE_RAM_LOG_QUEUE       48

#define STORAGE_LOG_MAX_FILE_BYTES  (2UL * 1024UL * 1024UL)
#define STORAGE_LOG_MAX_TOTAL_BYTES (100UL * 1024UL * 1024UL)
#define STORAGE_LOG_MAX_AGE_DAYS    30
#define STORAGE_LOG_FLUSH_NORMAL_MS 5000UL

#define STORAGE_LOW_SPACE_PCT       15
#define STORAGE_CRITICAL_SPACE_PCT  5

#define STORAGE_ASSET_CACHE_SLOTS   8

// Required directories (created if missing; never wipe user files)
static const char *const STORAGE_REQUIRED_DIRS[] = {
  "/showduino",
  "/showduino/system",
  "/showduino/ui",
  "/showduino/ui/backgrounds",
  "/showduino/ui/icons",
  "/showduino/ui/icons/navigation",
  "/showduino/ui/icons/transport",
  "/showduino/ui/icons/status",
  "/showduino/ui/icons/devices",
  "/showduino/ui/icons/shows",
  "/showduino/ui/icons/lighting",
  "/showduino/ui/icons/audio",
  "/showduino/ui/icons/video",
  "/showduino/ui/icons/effects",
  "/showduino/ui/icons/timeline",
  "/showduino/ui/icons/system",
  "/showduino/ui/icons/alerts",
  "/showduino/ui/overlays",
  "/showduino/ui/animations",
  "/showduino/ui/fonts",
  "/showduino/ui/sounds",
  "/showduino/shows",
  "/showduino/shows/packages",
  "/showduino/shows/trash",
  "/showduino/devices",
  "/showduino/devices/fixture_profiles",
  "/showduino/devices/presets",
  "/showduino/logs",
  "/showduino/logs/system",
  "/showduino/logs/communication",
  "/showduino/logs/shows",
  "/showduino/logs/warnings",
  "/showduino/logs/emergency",
  "/showduino/logs/crashes",
  "/showduino/backups",
  "/showduino/backups/automatic",
  "/showduino/backups/manual",
  "/showduino/updates",
  "/showduino/updates/pending",
  "/showduino/updates/installed",
  "/showduino/updates/failed",
  "/showduino/exports",
  "/showduino/exports/diagnostics",
  "/showduino/exports/logs",
  "/showduino/exports/shows",
  "/showduino/exports/configuration",
  "/showduino/temp",
  "/showduino/www",
  nullptr
};

#define PATH_DIRECTOR_JSON   "/showduino/system/director.json"
#define PATH_SETTINGS_JSON   "/showduino/system/settings.json"
#define PATH_NETWORK_JSON    "/showduino/system/network.json"
#define PATH_STARTUP_JSON    "/showduino/system/startup.json"
#define PATH_STORAGE_JSON    "/showduino/system/storage.json"
#define PATH_VERSION_JSON    "/showduino/system/version.json"
#define PATH_ASSET_MANIFEST  "/showduino/ui/asset_manifest.json"
#define PATH_SHOW_INDEX      "/showduino/shows/index.json"
#define PATH_SHOW_FAVOURITES "/showduino/shows/favourites.json"
#define PATH_SHOW_RECENT     "/showduino/shows/recent.json"
#define PATH_PAIRED_DEVICES  "/showduino/devices/paired_devices.json"
#define PATH_STAGE_CONTROLLERS "/showduino/devices/stage_controllers.json"
#define PATH_NODES_JSON      "/showduino/devices/nodes.json"
#define PATH_WEBUI_WWW       "/showduino/www"

#endif
