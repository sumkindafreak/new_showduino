#pragma once

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "Service.h"
#include "../OsTypes.h"
#include "../models/Production.h"
#include "../events/EventBus.h"
#include "../Compatibility.h"

namespace Os2 {

/**
 * AssetService — Compatibility contract v1 (Api::AssetService).
 * Production catalogue & metadata only. No paths to apps. No runtime.
 * Load intent → CommandService.execute(LoadProduction).
 */
class AssetService : public IService {
 public:
  static constexpr uint16_t kApiVersion = Api::AssetService;
  static constexpr int kMaxProductions = 32;

  const char *id() const override { return "asset"; }

  void begin() override {
    count_ = 0;
    selected_[0] = '\0';
    revision_ = 0;
  }

  /** Replace entire catalogue (communication / storage bridge only). */
  void setCatalogue(const ProductionManifest *items, int n) {
    count_ = 0;
    if (!items || n <= 0) {
      bump();
      events().publish(Event::CatalogueChanged, 0);
      return;
    }
    if (n > kMaxProductions) n = kMaxProductions;
    for (int i = 0; i < n; ++i) {
      productions_[i] = items[i];
    }
    count_ = n;
    bump();
    events().publish(Event::CatalogueChanged, (uint32_t)count_);
  }

  int count() const { return count_; }

  const ProductionManifest *at(int i) const {
    return (i >= 0 && i < count_) ? &productions_[i] : nullptr;
  }

  const ProductionManifest *find(const char *productionId) const {
    if (!productionId || !productionId[0]) return nullptr;
    for (int i = 0; i < count_; ++i) {
      if (strcmp(productions_[i].id, productionId) == 0) return &productions_[i];
    }
    return nullptr;
  }

  /** Case-insensitive substring match on name / id / author. Returns hit count. */
  int search(const char *query, const ProductionManifest **out, int maxOut) const {
    if (!out || maxOut <= 0) return 0;
    if (!query || !query[0]) {
      int n = count_ < maxOut ? count_ : maxOut;
      for (int i = 0; i < n; ++i) out[i] = &productions_[i];
      return n;
    }
    int hits = 0;
    for (int i = 0; i < count_ && hits < maxOut; ++i) {
      if (matchField(productions_[i].name, query) ||
          matchField(productions_[i].id, query) ||
          matchField(productions_[i].author, query) ||
          matchField(productions_[i].description, query)) {
        out[hits++] = &productions_[i];
      }
    }
    return hits;
  }

  void select(const char *productionId) {
    if (!productionId) productionId = "";
    if (strncmp(selected_, productionId, sizeof(selected_)) == 0) return;
    strncpy(selected_, productionId, sizeof(selected_) - 1);
    selected_[sizeof(selected_) - 1] = '\0';
    bump();
    events().publish(Event::ProductionSelected);
  }

  const char *selectedId() const { return selected_; }

  const ProductionManifest *selected() const {
    return selected_[0] ? find(selected_) : nullptr;
  }

  uint32_t revision() const { return revision_; }

 private:
  void bump() { ++revision_; }

  static bool matchField(const char *field, const char *query) {
    if (!field || !query) return false;
    /* naive case-insensitive substring */
    for (const char *p = field; *p; ++p) {
      const char *a = p;
      const char *b = query;
      while (*a && *b &&
             tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        ++a; ++b;
      }
      if (!*b) return true;
    }
    return false;
  }

  ProductionManifest productions_[kMaxProductions]{};
  int count_ = 0;
  char selected_[64]{};
  uint32_t revision_ = 0;
};

}  // namespace Os2