#ifndef SHOWDUINO_DISPLAY_PAGES_H
#define SHOWDUINO_DISPLAY_PAGES_H

#include "DisplayTypes.h"

static const OverlayId kModalOverlays[] = {
  OVERLAY_CLOCK,
  OVERLAY_DATE,
  OVERLAY_FOOTER,
  OVERLAY_LINK,
  OVERLAY_SAFETY,
  OVERLAY_STAGE,
};

/* Page 01 Home: LVGL tiles only. Header / footer owned by page_01_home. */
static const DisplayPage kDesktopPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, true, true, /* hideDock, hybridPanel */
};

/* Page 02 Productions: LVGL library shell. */
static const DisplayPage kShowsPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, true, true, /* hideDock, hybridPanel */
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

static const TouchRegion kDiscoveryTouchRegions[] = {
  {{120, 300, 680, 380}, "UI:DISCOVERY:SCAN", 0},
  {{120, 390, 680, 450}, "SCREEN:NODES", 0},
  {{280, 450, 520, 478}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kDiscoveryPage = {
  nullptr, kDiscoveryTouchRegions,
  (uint16_t)(sizeof(kDiscoveryTouchRegions) / sizeof(kDiscoveryTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kLockedTouchRegions[] = {
  {{80, 280, 720, 400}, "UI:LOCK:UNLOCK", 0},
};

static const DisplayPage kLockedPage = {
  nullptr, kLockedTouchRegions,
  (uint16_t)(sizeof(kLockedTouchRegions) / sizeof(kLockedTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kUnlockTouchRegions[] = {
  {{80, 280, 720, 400}, "UI:LOCK:CONFIRM", 0},
  {{80, 410, 360, 470}, "UI:LOCK:CANCEL", 0},
};

static const DisplayPage kUnlockPage = {
  nullptr, kUnlockTouchRegions,
  (uint16_t)(sizeof(kUnlockTouchRegions) / sizeof(kUnlockTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const DisplayPage kEmergencyPage = {
  nullptr, nullptr, 0,
  nullptr, 0,
  false, false, 0, true, false,
};

static const TouchRegion kConnectionLostTouchRegions[] = {
  {{40, 360, 260, 430}, "UI:NET:RETRY", 0},
  {{280, 360, 500, 430}, "UI:NET:SCAN", 0},
  {{520, 360, 760, 430}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kConnectionLostPage = {
  nullptr, kConnectionLostTouchRegions,
  (uint16_t)(sizeof(kConnectionLostTouchRegions) / sizeof(kConnectionLostTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kNoNetworkTouchRegions[] = {
  {{40, 360, 260, 430}, "UI:NET:RETRY", 0},
  {{280, 360, 500, 430}, "UI:NET:SCAN", 0},
  {{520, 360, 760, 430}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kNoNetworkPage = {
  nullptr, kNoNetworkTouchRegions,
  (uint16_t)(sizeof(kNoNetworkTouchRegions) / sizeof(kNoNetworkTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kNoSdTouchRegions[] = {
  {{200, 360, 600, 430}, "STORAGE:REPAIR", 0},
  {{280, 440, 520, 478}, "UI:SYSTEM:REBOOT", 0},
};

static const DisplayPage kNoSdPage = {
  nullptr, kNoSdTouchRegions,
  (uint16_t)(sizeof(kNoSdTouchRegions) / sizeof(kNoSdTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kRebootTouchRegions[] = {
  {{280, 440, 520, 478}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kRebootPage = {
  nullptr, kRebootTouchRegions,
  (uint16_t)(sizeof(kRebootTouchRegions) / sizeof(kRebootTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kFirmwareUpdateTouchRegions[] = {
  {{280, 440, 520, 478}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kFirmwareUpdatePage = {
  nullptr, kFirmwareUpdateTouchRegions,
  (uint16_t)(sizeof(kFirmwareUpdateTouchRegions) / sizeof(kFirmwareUpdateTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kBackupTouchRegions[] = {
  {{120, 360, 380, 430}, "STORAGE:BACKUP", 0},
  {{420, 360, 680, 430}, "SCREEN:DESKTOP", 0},
};

static const DisplayPage kBackupPage = {
  nullptr, kBackupTouchRegions,
  (uint16_t)(sizeof(kBackupTouchRegions) / sizeof(kBackupTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kRecoveryTouchRegions[] = {
  {{120, 360, 380, 430}, "STORAGE:REPAIR", 0},
  {{420, 360, 680, 430}, "UI:SYSTEM:REBOOT", 0},
};

static const DisplayPage kRecoveryPage = {
  nullptr, kRecoveryTouchRegions,
  (uint16_t)(sizeof(kRecoveryTouchRegions) / sizeof(kRecoveryTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

static const TouchRegion kCompleteTouchRegions[] = {
  {{200, 360, 600, 430}, "UI:COMPLETE:MENU", 0},
  {{120, 300, 320, 360}, "UI:COMPLETE:RUN", 0},
  {{480, 300, 680, 360}, "UI:COMPLETE:EXPORT", 0},
};

static const DisplayPage kCompletePage = {
  nullptr, kCompleteTouchRegions,
  (uint16_t)(sizeof(kCompleteTouchRegions) / sizeof(kCompleteTouchRegions[0])),
  kModalOverlays, (uint16_t)(sizeof(kModalOverlays) / sizeof(kModalOverlays[0])),
  false, false, 0, true, false,
};

inline const DisplayPage *displayPageById(DisplayPageId id) {
  switch (id) {
    case PAGE_DESKTOP: return &kDesktopPage;
    case PAGE_SHOWS: return &kShowsPage;
    case PAGE_LIVE: return &kLivePage;
    case PAGE_DIAGNOSTICS: return &kDiagnosticsPage;
    case PAGE_NODES: return &kNodesPage;
    case PAGE_LOCKED: return &kLockedPage;
    case PAGE_UNLOCK: return &kUnlockPage;
    case PAGE_EMERGENCY: return &kEmergencyPage;
    case PAGE_CONNECTION_LOST: return &kConnectionLostPage;
    case PAGE_NO_NETWORK: return &kNoNetworkPage;
    case PAGE_NO_SD: return &kNoSdPage;
    case PAGE_REBOOT: return &kRebootPage;
    case PAGE_FIRMWARE_UPDATE: return &kFirmwareUpdatePage;
    case PAGE_BACKUP: return &kBackupPage;
    case PAGE_RECOVERY: return &kRecoveryPage;
    case PAGE_DISCOVERY: return &kDiscoveryPage;
    case PAGE_COMPLETE: return &kCompletePage;
    default: return nullptr;
  }
}

inline const DisplayPage *displayPageDesktop() { return &kDesktopPage; }

inline const char *displayPageTitle(DisplayPageId id) {
  switch (id) {
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
    default: return "SHOWDUINO";
  }
}

#endif