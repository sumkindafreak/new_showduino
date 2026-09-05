#ifndef SHOWDUINO_DISPLAY_SYSTEM_PAGES_H
#define SHOWDUINO_DISPLAY_SYSTEM_PAGES_H

#include "DisplayTypes.h"

/**
 * Copy + actions for full-screen system pages.
 * Visual language matches DirectorUnlockScreen / ShowduinoOsUi.
 * Commands are unchanged from the previous BMP-region map.
 */
struct SystemPageAction {
  const char *command;
  const char *label;
  bool danger;
};

struct SystemPageSpec {
  const char *kicker;
  const char *title;
  const char *subtitle;
  const char *body;
  const SystemPageAction *actions;
  uint8_t actionCount;
  bool dangerAccent;
};

inline const SystemPageSpec *displaySystemPageSpec(DisplayPageId id) {
  static const SystemPageAction kLockedActions[] = {
    {"UI:LOCK:UNLOCK", "UNLOCK", false},
  };
  static const SystemPageAction kUnlockActions[] = {
    {"UI:LOCK:CONFIRM", "CONFIRM UNLOCK", false},
    {"UI:LOCK:CANCEL", "CANCEL", false},
  };
  static const SystemPageAction kLinkActions[] = {
    {"UI:NET:RETRY", "RETRY LINK", false},
    {"SCREEN:DESKTOP", "DESKTOP", false},
  };
  static const SystemPageAction kNoSdActions[] = {
    {"STORAGE:REPAIR", "REPAIR STORAGE", false},
    {"UI:SYSTEM:REBOOT", "REBOOT", true},
  };
  static const SystemPageAction kRebootActions[] = {
    {"SCREEN:DESKTOP", "DESKTOP", false},
  };
  static const SystemPageAction kFwActions[] = {
    {"SCREEN:DESKTOP", "DESKTOP", false},
  };
  static const SystemPageAction kBackupActions[] = {
    {"STORAGE:BACKUP", "BACKUP NOW", false},
    {"SCREEN:DESKTOP", "DESKTOP", false},
  };
  static const SystemPageAction kRecoveryActions[] = {
    {"STORAGE:REPAIR", "REPAIR", false},
    {"UI:SYSTEM:REBOOT", "REBOOT", true},
  };
  static const SystemPageAction kDiscoveryActions[] = {
    {"SCREEN:NODES", "NODES", false},
    {"SCREEN:DESKTOP", "DESKTOP", false},
  };
  static const SystemPageAction kCompleteActions[] = {
    {"UI:COMPLETE:RUN", "RUN AGAIN", false},
    {"UI:COMPLETE:MENU", "RETURN HOME", false},
  };

  static const SystemPageSpec kLocked = {
    "///  DIRECTOR SYSTEM",
    "DIRECTOR LOCKED",
    "OPERATOR LOCK",
    "The control surface is locked. Show state on Stage is unchanged. Unlock to operate.",
    kLockedActions, 1, false
  };
  static const SystemPageSpec kUnlock = {
    "///  DIRECTOR SYSTEM",
    "UNLOCK DIRECTOR",
    "CONFIRM ACCESS",
    "Unlocking returns operator control. Stage remains the show authority.",
    kUnlockActions, 2, false
  };
  static const SystemPageSpec kLinkLost = {
    "///  COMMUNICATIONS",
    "CONNECTION LOST",
    "STAGE LINK DOWN",
    "The Director is not receiving Stage state. Outputs stay as last commanded by Stage. Retry when SUE is reachable.",
    kLinkActions, 2, true
  };
  static const SystemPageSpec kNoNet = {
    "///  COMMUNICATIONS",
    "NO NETWORK",
    "FABRIC UNAVAILABLE",
    "ESP-NOW has not joined the show fabric. The Director can still display local pages. Retry discovery.",
    kLinkActions, 2, true
  };
  static const SystemPageSpec kNoSd = {
    "///  STORAGE",
    "NO SD CARD",
    "LIBRARY UNAVAILABLE",
    "Production packages live on the Director SD card. Insert a card or repair the storage layout.",
    kNoSdActions, 2, true
  };
  static const SystemPageSpec kReboot = {
    "///  DIRECTOR SYSTEM",
    "SYSTEM REBOOT",
    "RESTARTING",
    "The Director is restarting. Stage keeps its last safe state. This panel does not command show playback.",
    kRebootActions, 1, false
  };
  static const SystemPageSpec kFw = {
    "///  DIRECTOR SYSTEM",
    "FIRMWARE UPDATE",
    "MAINTENANCE",
    "Firmware update is a Director-local operation. Stage remains authoritative for the show.",
    kFwActions, 1, false
  };
  static const SystemPageSpec kBackup = {
    "///  STORAGE",
    "BACKUP",
    "DIRECTOR ARCHIVE",
    "Create a local backup of Director storage. This does not copy Stage runtime state.",
    kBackupActions, 2, false
  };
  static const SystemPageSpec kRecovery = {
    "///  STORAGE",
    "RECOVERY",
    "REPAIR REQUIRED",
    "Storage layout is incomplete. Repair directories, then reboot if the library still will not load.",
    kRecoveryActions, 2, true
  };
  static const SystemPageSpec kDiscovery = {
    "///  FABRIC",
    "DISCOVERY",
    "LOOKING FOR NODES",
    "The Director displays nodes reported by Stage. Discovery does not bring equipment online by itself.",
    kDiscoveryActions, 2, false
  };
  static const SystemPageSpec kComplete = {
    "///  PRODUCTION",
    "SHOW COMPLETE",
    "STAGE REPORTS FINISHED",
    "Playback finished on Stage. The production remains loaded. Run again or return home.",
    kCompleteActions, 2, false
  };
  static const SystemPageSpec kEmergency = {
    "///  EMERGENCY STATE",
    "EMERGENCY",
    "SHOW STOPPED",
    "Emergency latch is active on Stage. Use the dedicated emergency screen to request CLEAR.",
    nullptr, 0, true
  };

  switch (id) {
    case PAGE_LOCKED: return &kLocked;
    case PAGE_UNLOCK: return &kUnlock;
    case PAGE_CONNECTION_LOST: return &kLinkLost;
    case PAGE_NO_NETWORK: return &kNoNet;
    case PAGE_NO_SD: return &kNoSd;
    case PAGE_REBOOT: return &kReboot;
    case PAGE_FIRMWARE_UPDATE: return &kFw;
    case PAGE_BACKUP: return &kBackup;
    case PAGE_RECOVERY: return &kRecovery;
    case PAGE_DISCOVERY: return &kDiscovery;
    case PAGE_COMPLETE: return &kComplete;
    case PAGE_EMERGENCY: return &kEmergency;
    default: return nullptr;
  }
}

#endif
