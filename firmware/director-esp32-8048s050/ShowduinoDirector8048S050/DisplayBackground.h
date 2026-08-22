#ifndef SHOWDUINO_DISPLAY_BACKGROUND_H
#define SHOWDUINO_DISPLAY_BACKGROUND_H

#include "DisplayTypes.h"

/**
 * Background image slot (retired). Theme BMPs are no longer loaded.
 * unload() still releases any leftover buffer.
 */
class DisplayBackground {
 public:
  void begin(DisplayStats *stats);
  void unload();
  /** Force unload so the next load misses cache (dev / SD file changed). */
  void invalidateCache() { unload(); }

  /**
   * Load path for page/theme. Cache hit if same page+theme+path already loaded.
   * PSRAM only — no internal SRAM fallback.
   */
  bool load(DisplayPageId page, const char *themeName, const char *absPath);

  bool isLoaded() const { return buf_ != nullptr; }
  DisplayPageId loadedPage() const { return loadedPage_; }
  const char *loadedTheme() const { return loadedTheme_; }
  const char *loadedPath() const { return loadedPath_; }

  lv_obj_t *bindToScreen(lv_obj_t *screen);
  void unbind();

  uint32_t fileSize() const { return fileSize_; }

 private:
  bool validateAndDecode(const char *path, uint32_t &decodeMs);

  uint16_t *buf_ = nullptr;
  size_t bufBytes_ = 0;
  lv_image_dsc_t dsc_{};
  lv_obj_t *imgObj_ = nullptr;
  DisplayPageId loadedPage_ = PAGE_NONE;
  char loadedTheme_[32] = {};
  char loadedPath_[96] = {};
  uint32_t fileSize_ = 0;
  DisplayStats *stats_ = nullptr;
};

#endif
