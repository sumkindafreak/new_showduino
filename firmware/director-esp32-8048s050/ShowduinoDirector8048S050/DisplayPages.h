#ifndef SHOWDUINO_DISPLAY_PAGES_H
#define SHOWDUINO_DISPLAY_PAGES_H

#include "DisplayTypes.h"

/* Hybrid LVGL operator pages — panel owned by ShowduinoUi / page_0x modules. */
static const DisplayPage kDesktopPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true, /* hideDock, hybridPanel */
};

static const DisplayPage kShowsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kShowDetailsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kLivePage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kDiagnosticsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kNodesPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kSettingsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kAudioPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

static const DisplayPage kLogsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, false, true,
};

/* Full-screen system pages — LVGL chrome, no dock, no HUD overlays. */
static const DisplayPage kSystemModalPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, true, false,
};

inline const DisplayPage *displayPageById(DisplayPageId id) {
  switch (id) {
    case PAGE_DESKTOP: return &kDesktopPage;
    case PAGE_SHOWS: return &kShowsPage;
    case PAGE_SHOW_DETAILS: return &kShowDetailsPage;
    case PAGE_LIVE: return &kLivePage;
    case PAGE_DIAGNOSTICS: return &kDiagnosticsPage;
    case PAGE_NODES: return &kNodesPage;
    case PAGE_SETTINGS: return &kSettingsPage;
    case PAGE_AUDIO: return &kAudioPage;
    case PAGE_LOGS: return &kLogsPage;
    case PAGE_LOCKED:
    case PAGE_UNLOCK:
    case PAGE_EMERGENCY:
    case PAGE_CONNECTION_LOST:
    case PAGE_NO_NETWORK:
    case PAGE_NO_SD:
    case PAGE_REBOOT:
    case PAGE_FIRMWARE_UPDATE:
    case PAGE_BACKUP:
    case PAGE_RECOVERY:
    case PAGE_DISCOVERY:
    case PAGE_COMPLETE:
      return &kSystemModalPage;
    default: return nullptr;
  }
}

inline const DisplayPage *displayPageDesktop() { return &kDesktopPage; }

inline bool displayPageIsSystemModal(DisplayPageId id) {
  switch (id) {
    case PAGE_LOCKED:
    case PAGE_UNLOCK:
    case PAGE_EMERGENCY:
    case PAGE_CONNECTION_LOST:
    case PAGE_NO_NETWORK:
    case PAGE_NO_SD:
    case PAGE_REBOOT:
    case PAGE_FIRMWARE_UPDATE:
    case PAGE_BACKUP:
    case PAGE_RECOVERY:
    case PAGE_DISCOVERY:
    case PAGE_COMPLETE:
      return true;
    default:
      return false;
  }
}

inline const char *displayPageTitle(DisplayPageId id) {
  switch (id) {
    case PAGE_DESKTOP: return "HOME";
    case PAGE_SHOWS: return "PRODUCTIONS";
    case PAGE_SHOW_DETAILS: return "SHOW DETAILS";
    case PAGE_LIVE: return "LIVE";
    case PAGE_SETTINGS: return "SETTINGS";
    case PAGE_AUDIO: return "AUDIO";
    case PAGE_LOGS: return "SYSTEM LOGS";
    case PAGE_LOCKED: return "DIRECTOR LOCKED";
    case PAGE_UNLOCK: return "UNLOCK DIRECTOR";
    case PAGE_CONNECTION_LOST: return "CONNECTION LOST";
    case PAGE_NO_NETWORK: return "NO NETWORK";
    case PAGE_NO_SD: return "NO SD CARD";
    case PAGE_REBOOT: return "SYSTEM REBOOT";
    case PAGE_FIRMWARE_UPDATE: return "FIRMWARE UPDATE";
    case PAGE_BACKUP: return "BACKUP";
    case PAGE_RECOVERY: return "RECOVERY";
    case PAGE_DISCOVERY: return "DISCOVERY";
    case PAGE_COMPLETE: return "SHOW COMPLETE";
    case PAGE_DIAGNOSTICS: return "DIAGNOSTICS";
    case PAGE_NODES: return "NODES";
    case PAGE_EMERGENCY: return "EMERGENCY";
    default: return "SHOWDUINO";
  }
}

#endif
