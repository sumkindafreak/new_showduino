#ifndef SHOWDUINO_PRODUCTION_STORE_H
#define SHOWDUINO_PRODUCTION_STORE_H

#include <Arduino.h>
#include <FS.h>

#include "ProductionFormat.h"

#define SHOWDUINO_PRODUCTIONS_ROOT       "/showduino/productions"
#define SHOWDUINO_PRODUCTION_MAX_FOUND   32U
#define SHOWDUINO_MANIFEST_MAX_BYTES     4096U
#define SHOWDUINO_TIMELINE_MAX_BYTES     (128U * 1024U)

enum class ProductionStoreResult : uint8_t {
  Ok = 0,
  StorageUnavailable,
  NotFound,
  MissingManifest,
  InvalidManifest,
  MissingTimeline,
  InvalidTimeline,
  UnsupportedVersion,
  DuplicateCueId,
  InvalidCueTime,
  UnsupportedCueType,
  InvalidCueAction,
  TooManyCues,
  CommandTooLong,
  TooLarge,
  TooManyProductions,
  NoMemory,
  IoError
};

struct ProductionPackage {
  ProductionManifest manifest;
  ProductionTimeline timeline;
  ProductionCue *cues = nullptr;
};

class ProductionStore {
public:
  bool begin(fs::FS &fs);
  bool scan();
  ProductionStoreResult load(const char *productionId, ProductionPackage *out);
  void release(ProductionPackage *package);
  void markLoaded(const ProductionManifest &manifest);
  void unload();

  uint8_t count() const { return foundCount_; }
  const ProductionManifest *at(uint8_t index) const;
  const ProductionManifest *find(const char *productionId) const;
  bool hasLoaded() const { return loaded_.productionId[0] != '\0'; }
  const ProductionManifest &loaded() const { return loaded_; }
  const char *lastError() const { return lastError_; }

private:
  fs::FS *fs_ = nullptr;
  ProductionManifest found_[SHOWDUINO_PRODUCTION_MAX_FOUND]{};
  uint8_t foundCount_ = 0;
  ProductionManifest loaded_{};
  char lastError_[96] = "not initialized";

  ProductionStoreResult readManifest(const char *folder, ProductionManifest *out);
  bool readText(const char *path, size_t maxBytes, char **out, size_t *outLen);
  void setError(const char *message);
};

const char *productionStoreResultName(ProductionStoreResult result);

#endif
