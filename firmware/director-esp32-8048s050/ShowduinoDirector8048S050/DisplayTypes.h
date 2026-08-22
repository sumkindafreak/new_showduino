#ifndef SHOWDUINO_DISPLAY_TYPES_H
#define SHOWDUINO_DISPLAY_TYPES_H

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"

#ifndef SHOWDUINO_DISPLAY_STATS
#define SHOWDUINO_DISPLAY_STATS 1
#endif

constexpr uint16_t DISPLAY_WIDTH  = (uint16_t)SCREEN_WIDTH;
constexpr uint16_t DISPLAY_HEIGHT = (uint16_t)SCREEN_HEIGHT;
constexpr int DISPLAY_THEME_MAJOR = 1;

enum DisplayPageId : uint8_t {
  PAGE_NONE = 0,
  PAGE_DESKTOP,
  PAGE_LIVE,
  PAGE_SHOWS,
  PAGE_SHOW_DETAILS,
  PAGE_AUDIO,
  PAGE_SETTINGS,
  PAGE_NODES,
  PAGE_DIAGNOSTICS,
  PAGE_LOGS,
  PAGE_ABOUT,
  PAGE_HELP,
  /* Full-screen / modal pages (LVGL chrome; no theme BMP) */
  PAGE_LOCKED,
  PAGE_UNLOCK,
  PAGE_EMERGENCY,
  PAGE_CONNECTION_LOST,
  PAGE_NO_NETWORK,
  PAGE_NO_SD,
  PAGE_REBOOT,
  PAGE_FIRMWARE_UPDATE,
  PAGE_BACKUP,
  PAGE_RECOVERY,
  PAGE_DISCOVERY,
  PAGE_COMPLETE,
  PAGE_COUNT
};

/* Permanent compositor layers, back to front. */
enum DisplayLayerId : uint8_t {
  DISPLAY_LAYER_BACKGROUND = 0,
  DISPLAY_LAYER_WIDGETS,
  DISPLAY_LAYER_CONTROLS,
  DISPLAY_LAYER_TEMPORARY,
  DISPLAY_LAYER_COUNT
};

enum DisplayState : uint8_t {
  DISPLAY_LOADING = 0,
  DISPLAY_READY,
  DISPLAY_TRANSITION,
  DISPLAY_ERROR
};

enum OverlayId : uint8_t {
  OVERLAY_CLOCK = 0,
  OVERLAY_DATE,
  OVERLAY_SHOW,
  OVERLAY_STAGE,
  OVERLAY_LINK,
  OVERLAY_SAFETY,
  OVERLAY_NODECOUNT,
  OVERLAY_CUE,
  OVERLAY_ELAPSED,
  OVERLAY_REMAIN,
  OVERLAY_FOOTER,
  OVERLAY_NOTIFICATION,
  OVERLAY_COUNT
};

/* Immutable for the duration of a single updateWidgets() call. */
struct DisplaySnapshot {
  char clock[16];
  char date[28];
  char currentShow[64];
  char runtimeState[24];
  char safetyState[24];
  char linkState[24];
  char cue[24];
  char elapsed[16];
  char remain[16];
  char footer[96];
  char notification[64];
  uint16_t nodeCount;
  uint8_t progressPct;
  DisplayPageId page;
};

struct TouchRegion {
  lv_area_t bounds;
  const char *command;
  uint32_t flags;
};

struct DisplayPage {
  const char *imageBasename;
  const TouchRegion *regions;
  uint16_t regionCount;
  const OverlayId *overlays;
  uint16_t overlayCount;
  bool useBackgroundImage;
  bool animated;
  uint16_t frameRate;
  /** Full-screen HUD — hide the LVGL bottom dock. */
  bool hideDock;
  /** Transparent LVGL page panel over the background (e.g. Live relays). */
  bool hybridPanel;
};

struct DisplayCapabilities {
  bool psram;
  bool bmp;
  bool png;
  bool jpeg;
  bool animations;
};

/* All timings are milliseconds measured with millis(). */
struct DisplayStats {
  uint32_t loadTimeMs;
  uint32_t bmpDecodeMs;
  uint32_t overlayUpdateMs;
  uint32_t redrawCount;
  uint32_t cacheHits;
  uint32_t cacheMisses;
};

inline void displaySnapshotClear(DisplaySnapshot &s) {
  s.clock[0] = '\0';
  s.date[0] = '\0';
  s.currentShow[0] = '\0';
  s.runtimeState[0] = '\0';
  s.safetyState[0] = '\0';
  s.linkState[0] = '\0';
  s.cue[0] = '\0';
  s.elapsed[0] = '\0';
  s.remain[0] = '\0';
  s.footer[0] = '\0';
  s.notification[0] = '\0';
  s.nodeCount = 0;
  s.progressPct = 0;
  s.page = PAGE_NONE;
}

#endif
