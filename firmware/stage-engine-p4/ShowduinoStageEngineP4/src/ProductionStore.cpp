#include "ProductionStore.h"

#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

namespace {

static const char *baseName(const char *path) {
  if (!path) return "";
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static ProductionStoreResult mapParseResult(ProductionParseResult result,
                                             bool manifest) {
  switch (result) {
    case ProductionParseResult::UnsupportedVersion:
      return ProductionStoreResult::UnsupportedVersion;
    case ProductionParseResult::DuplicateCueId:
      return ProductionStoreResult::DuplicateCueId;
    case ProductionParseResult::InvalidCueTime:
      return ProductionStoreResult::InvalidCueTime;
    case ProductionParseResult::UnsupportedCueType:
      return ProductionStoreResult::UnsupportedCueType;
    case ProductionParseResult::InvalidCueAction:
      return ProductionStoreResult::InvalidCueAction;
    case ProductionParseResult::TooManyCues:
      return ProductionStoreResult::TooManyCues;
    case ProductionParseResult::CommandTooLong:
      return ProductionStoreResult::CommandTooLong;
    default:
      return manifest ? ProductionStoreResult::InvalidManifest
                      : ProductionStoreResult::InvalidTimeline;
  }
}

} // namespace

const char *productionStoreResultName(ProductionStoreResult result) {
  switch (result) {
    case ProductionStoreResult::Ok: return "OK";
    case ProductionStoreResult::StorageUnavailable: return "STORAGE_UNAVAILABLE";
    case ProductionStoreResult::NotFound: return "NOT_FOUND";
    case ProductionStoreResult::MissingManifest: return "MISSING_MANIFEST";
    case ProductionStoreResult::InvalidManifest: return "INVALID_MANIFEST";
    case ProductionStoreResult::MissingTimeline: return "MISSING_TIMELINE";
    case ProductionStoreResult::InvalidTimeline: return "INVALID_TIMELINE";
    case ProductionStoreResult::UnsupportedVersion: return "UNSUPPORTED_VERSION";
    case ProductionStoreResult::DuplicateCueId: return "DUPLICATE_CUE_ID";
    case ProductionStoreResult::InvalidCueTime: return "INVALID_CUE_TIME";
    case ProductionStoreResult::UnsupportedCueType: return "UNSUPPORTED_CUE_TYPE";
    case ProductionStoreResult::InvalidCueAction: return "INVALID_CUE_ACTION";
    case ProductionStoreResult::TooManyCues: return "TOO_MANY_CUES";
    case ProductionStoreResult::CommandTooLong: return "COMMAND_TOO_LONG";
    case ProductionStoreResult::TooLarge: return "FILE_TOO_LARGE";
    case ProductionStoreResult::TooManyProductions: return "TOO_MANY_PRODUCTIONS";
    case ProductionStoreResult::NoMemory: return "NO_MEMORY";
    case ProductionStoreResult::IoError: return "IO_ERROR";
    default: return "IO_ERROR";
  }
}

void ProductionStore::setError(const char *message) {
  strncpy(lastError_, message ? message : "unknown", sizeof(lastError_) - 1);
  lastError_[sizeof(lastError_) - 1] = '\0';
}

bool ProductionStore::begin(fs::FS &fs) {
  fs_ = &fs;
  foundCount_ = 0;
  for (ProductionManifest &manifest : found_) manifest = ProductionManifest{};
  loaded_ = ProductionManifest{};
  Serial.println("[PRODUCTION] Initializing storage...");
  Serial.printf("[PRODUCTION] Directory: %s\n", SHOWDUINO_PRODUCTIONS_ROOT);
  if (!fs_->exists(SHOWDUINO_PRODUCTIONS_ROOT) &&
      !fs_->mkdir(SHOWDUINO_PRODUCTIONS_ROOT)) {
    setError("productions directory unavailable");
    Serial.println("[PRODUCTION] ERROR: productions directory unavailable");
    return false;
  }
  Serial.println("[PRODUCTION] SD ready");
  return scan();
}

bool ProductionStore::scan() {
  foundCount_ = 0;
  for (ProductionManifest &manifest : found_) manifest = ProductionManifest{};
  if (!fs_) {
    setError("storage unavailable");
    return false;
  }
  File root = fs_->open(SHOWDUINO_PRODUCTIONS_ROOT);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    setError("productions directory unavailable");
    return false;
  }

  File entry = root.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      const char *folder = baseName(entry.name());
      if (productionIdIsValid(folder)) {
        ProductionManifest manifest;
        ProductionStoreResult result = readManifest(folder, &manifest);
        if (result == ProductionStoreResult::Ok &&
            strcmp(folder, manifest.productionId) == 0) {
          if (foundCount_ < SHOWDUINO_PRODUCTION_MAX_FOUND) {
            found_[foundCount_++] = manifest;
            Serial.printf("[PRODUCTION] Found: %s\n", manifest.productionId);
          } else {
            setError("production discovery limit reached");
            Serial.println("[PRODUCTION] WARNING: production discovery limit reached");
            entry.close();
            break;
          }
        } else {
          Serial.printf("[PRODUCTION] Skipped invalid folder: %s (%s)\n",
                        folder, productionStoreResultName(result));
        }
      }
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
  Serial.printf("[PRODUCTION] Valid productions: %u\n", (unsigned)foundCount_);
  setError("OK");
  return true;
}

const ProductionManifest *ProductionStore::at(uint8_t index) const {
  return index < foundCount_ ? &found_[index] : nullptr;
}

const ProductionManifest *ProductionStore::find(const char *productionId) const {
  if (!productionId) return nullptr;
  for (uint8_t i = 0; i < foundCount_; ++i) {
    if (strcmp(found_[i].productionId, productionId) == 0) return &found_[i];
  }
  return nullptr;
}

bool ProductionStore::readText(const char *path, size_t maxBytes, char **out,
                               size_t *outLen) {
  if (!fs_ || !path || !out || !outLen) return false;
  *out = nullptr;
  *outLen = 0;
  File file = fs_->open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return false;
  }
  size_t size = file.size();
  if (size == 0 || size > maxBytes) {
    file.close();
    return false;
  }
  char *buffer = (char *)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buffer) buffer = (char *)malloc(size + 1);
  if (!buffer) {
    file.close();
    return false;
  }
  size_t read = file.readBytes(buffer, size);
  file.close();
  if (read != size) {
    heap_caps_free(buffer);
    return false;
  }
  buffer[size] = '\0';
  *out = buffer;
  *outLen = size;
  return true;
}

ProductionStoreResult ProductionStore::readManifest(const char *folder,
                                                     ProductionManifest *out) {
  if (!fs_ || !folder || !out) return ProductionStoreResult::StorageUnavailable;
  char path[160];
  snprintf(path, sizeof(path), "%s/%s/manifest.json", SHOWDUINO_PRODUCTIONS_ROOT, folder);
  if (!fs_->exists(path)) return ProductionStoreResult::MissingManifest;
  File sizeProbe = fs_->open(path, FILE_READ);
  if (!sizeProbe) {
    setError("manifest open failed");
    return ProductionStoreResult::IoError;
  }
  size_t size = sizeProbe.size();
  sizeProbe.close();
  if (size == 0) {
    setError("empty manifest");
    return ProductionStoreResult::InvalidManifest;
  }
  if (size > SHOWDUINO_MANIFEST_MAX_BYTES) {
    setError("manifest exceeds size limit");
    return ProductionStoreResult::TooLarge;
  }
  char *json = nullptr;
  size_t jsonLen = 0;
  if (!readText(path, SHOWDUINO_MANIFEST_MAX_BYTES, &json, &jsonLen)) {
    setError("manifest read failed");
    return ProductionStoreResult::IoError;
  }
  ProductionParseResult parseResult = ProductionParseResult::Ok;
  bool ok = productionParseManifest(json, jsonLen, out, &parseResult);
  heap_caps_free(json);
  if (!ok) {
    setError(productionParseResultName(parseResult));
    return mapParseResult(parseResult, true);
  }
  if (strcmp(folder, out->productionId) != 0) {
    setError("manifest productionId does not match folder");
    return ProductionStoreResult::InvalidManifest;
  }
  return ProductionStoreResult::Ok;
}

ProductionStoreResult ProductionStore::load(const char *productionId,
                                             ProductionPackage *out) {
  if (!out) return ProductionStoreResult::IoError;
  release(out);
  if (!fs_) {
    setError("STORAGE_UNAVAILABLE");
    return ProductionStoreResult::StorageUnavailable;
  }
  if (!productionIdIsValid(productionId)) {
    setError("NOT_FOUND");
    return ProductionStoreResult::NotFound;
  }
  char folder[128];
  snprintf(folder, sizeof(folder), "%s/%s", SHOWDUINO_PRODUCTIONS_ROOT, productionId);
  File folderFile = fs_->open(folder);
  if (!folderFile || !folderFile.isDirectory()) {
    if (folderFile) folderFile.close();
    setError("production folder not found");
    return ProductionStoreResult::NotFound;
  }
  folderFile.close();

  Serial.printf("[PRODUCTION] Loading: %s\n", productionId);
  ProductionStoreResult manifestResult = readManifest(productionId, &out->manifest);
  if (manifestResult != ProductionStoreResult::Ok) {
    setError(productionStoreResultName(manifestResult));
    return manifestResult;
  }
  Serial.println("[PRODUCTION] Manifest valid");
  Serial.printf("[PRODUCTION] Timeline: %s\n", out->manifest.timeline);

  char timelinePath[192];
  snprintf(timelinePath, sizeof(timelinePath), "%s/%s/%s",
           SHOWDUINO_PRODUCTIONS_ROOT, productionId, out->manifest.timeline);
  if (!fs_->exists(timelinePath)) {
    setError("MISSING_TIMELINE");
    return ProductionStoreResult::MissingTimeline;
  }
  File sizeProbe = fs_->open(timelinePath, FILE_READ);
  if (!sizeProbe) {
    setError("timeline open failed");
    return ProductionStoreResult::IoError;
  }
  size_t timelineSize = sizeProbe.size();
  sizeProbe.close();
  if (timelineSize == 0) {
    setError("empty timeline");
    return ProductionStoreResult::InvalidTimeline;
  }
  if (timelineSize > SHOWDUINO_TIMELINE_MAX_BYTES) {
    setError("FILE_TOO_LARGE");
    return ProductionStoreResult::TooLarge;
  }

  char *json = nullptr;
  size_t jsonLen = 0;
  if (!readText(timelinePath, SHOWDUINO_TIMELINE_MAX_BYTES, &json, &jsonLen)) {
    setError("timeline read failed");
    return ProductionStoreResult::IoError;
  }
  out->cues = (ProductionCue *)heap_caps_calloc(
      SHOWDUINO_PRODUCTION_MAX_CUES, sizeof(ProductionCue),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out->cues) {
    out->cues = (ProductionCue *)calloc(SHOWDUINO_PRODUCTION_MAX_CUES,
                                        sizeof(ProductionCue));
  }
  if (!out->cues) {
    heap_caps_free(json);
    setError("not enough memory for timeline");
    return ProductionStoreResult::NoMemory;
  }

  Serial.println("[TIMELINE] Parsing cues...");
  ProductionParseResult parseResult = ProductionParseResult::Ok;
  bool ok = productionParseTimeline(json, jsonLen, out->cues,
                                    SHOWDUINO_PRODUCTION_MAX_CUES,
                                    &out->timeline, &parseResult);
  heap_caps_free(json);
  if (!ok) {
    setError(productionParseResultName(parseResult));
    release(out);
    return mapParseResult(parseResult, false);
  }
  Serial.printf("[TIMELINE] %u cues validated\n", (unsigned)out->timeline.cueCount);
  setError("OK");
  return ProductionStoreResult::Ok;
}

void ProductionStore::release(ProductionPackage *package) {
  if (!package) return;
  if (package->cues) heap_caps_free(package->cues);
  *package = ProductionPackage{};
}

void ProductionStore::markLoaded(const ProductionManifest &manifest) {
  loaded_ = manifest;
}

void ProductionStore::unload() {
  loaded_ = ProductionManifest{};
}
