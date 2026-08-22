#ifndef SHOWDUINO_DISPLAY_OVERLAY_H
#define SHOWDUINO_DISPLAY_OVERLAY_H

#include "DisplayTypes.h"

class DisplayOverlay {
 public:
  void begin(lv_obj_t *parent, DisplayStats *stats);
  void clear();

  void setLayoutPage(DisplayPageId page);
  void setActiveOverlays(const OverlayId *ids, uint16_t count);
  void applySnapshot(const DisplaySnapshot &snap);
  void invalidateRegion(const lv_area_t &area);

 private:
  struct OverlayGeom {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    bool showBar;
  };

  void ensureWidgets();
  void applyGeometry();
  void styleThemedLabel(lv_obj_t *lab, bool accent = false);
  void setLabelIfChanged(OverlayId id, const char *text);
  static void overlayGeomForPage(DisplayPageId page, OverlayId id, OverlayGeom &g);

  lv_obj_t *parent_ = nullptr;
  lv_obj_t *labels_[OVERLAY_COUNT] = {};
  lv_obj_t *backs_[OVERLAY_COUNT] = {};
  lv_obj_t *progressBar_ = nullptr;
  lv_obj_t *progressBack_ = nullptr;
  char last_[OVERLAY_COUNT][96] = {};
  bool active_[OVERLAY_COUNT] = {};
  DisplayPageId layoutPage_ = PAGE_NONE;
  uint8_t lastProgress_ = 255;
  DisplayStats *stats_ = nullptr;
};

#endif
