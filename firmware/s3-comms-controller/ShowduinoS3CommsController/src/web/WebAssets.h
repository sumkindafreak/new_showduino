#ifndef SHOWDUINO_S3_WEB_ASSETS_H
#define SHOWDUINO_S3_WEB_ASSETS_H

#include "WebAssets.generated.h"
#include <string.h>

inline const ShowduinoWebAsset *commsWebFindAsset(const char *path) {
  if (!path || !path[0] || strcmp(path, "/") == 0) path = "/index.html";
  else if (strcmp(path, "/studio") == 0 || strcmp(path, "/studio/") == 0) path = "/studio/index.html";

  for (size_t i = 0; i < SHOWDUINO_WEBUI_ASSET_COUNT; i++) {
    if (strcmp(path, kShowduinoWebAssets[i].path) == 0) {
      return &kShowduinoWebAssets[i];
    }
  }
  return nullptr;
}

#endif
