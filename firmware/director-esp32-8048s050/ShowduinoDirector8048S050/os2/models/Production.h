#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../OsTypes.h"
#include "../Compatibility.h"

namespace Os2 {

/**
 * Production Manifest — Compatibility contract v1 (Api::ProductionManifest).
 *
 * Primary object of Showduino OS. Not a file path.
 * Library sees Productions; backends map storage behind AssetService.
 * Breaking field/meaning changes require Production Manifest v2.
 */

static constexpr uint16_t kProductionManifestApiVersion = Api::ProductionManifest;
struct ProductionCapabilities {
  bool audio;
  bool lighting;
  bool effects;
  uint16_t sceneCount;
  uint16_t audioTrackCount;
};

struct ProductionManifest {
  char id[64];
  char name[64];
  char description[96];
  char version[16];
  char author[48];
  uint32_t durationSeconds;
  bool hasThumbnail;
  ProductionCapabilities capabilities;
  char entryShow[64];   /* runtime entry — usually same as id */
  char lastEdited[32];  /* display string from package */
  StatusLevel readiness; /* Ready / Warning / … for operator */
};

enum class ProductionReadiness : uint8_t {
  Unknown = 0,
  Ready,
  Warning,
  Invalid
};

inline void productionClear(ProductionManifest &p) {
  memset(&p, 0, sizeof(p));
  p.readiness = StatusLevel::Inactive;
}

inline void productionFormatDuration(const ProductionManifest &p, char *buf, size_t n) {
  if (!buf || n == 0) return;
  uint32_t sec = p.durationSeconds;
  if (sec == 0) {
    snprintf(buf, n, "—");
    return;
  }
  uint32_t mins = sec / 60;
  uint32_t rem = sec % 60;
  if (mins >= 60) {
    snprintf(buf, n, "%luh %lum", (unsigned long)(mins / 60), (unsigned long)(mins % 60));
  } else if (rem == 0) {
    snprintf(buf, n, "%lu min", (unsigned long)mins);
  } else {
    snprintf(buf, n, "%lu:%02lu", (unsigned long)mins, (unsigned long)rem);
  }
}

inline const char *productionReadinessLabel(StatusLevel s) {
  switch (s) {
    case StatusLevel::Healthy:  return "Ready";
    case StatusLevel::Warning:  return "Warning";
    case StatusLevel::Critical: return "Invalid";
    case StatusLevel::Working:  return "Checking";
    default:                    return "Unknown";
  }
}

}  // namespace Os2