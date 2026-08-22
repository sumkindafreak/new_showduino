#include "DisplayBackground.h"

#include <esp_heap_caps.h>
#include <string.h>

void DisplayBackground::begin(DisplayStats *stats) {
  stats_ = stats;
}

void DisplayBackground::unload() {
  unbind();
  if (buf_) {
    heap_caps_free(buf_);
    buf_ = nullptr;
  }
  bufBytes_ = 0;
  memset(&dsc_, 0, sizeof(dsc_));
  loadedPage_ = PAGE_NONE;
  loadedTheme_[0] = '\0';
  loadedPath_[0] = '\0';
  fileSize_ = 0;
}

void DisplayBackground::unbind() {
  if (imgObj_) {
    lv_obj_delete(imgObj_);
    imgObj_ = nullptr;
  }
}

lv_obj_t *DisplayBackground::bindToScreen(lv_obj_t * /*screen*/) {
  return nullptr;
}

bool DisplayBackground::load(DisplayPageId /*page*/, const char * /*themeName*/,
                             const char * /*absPath*/) {
  return false;
}

bool DisplayBackground::validateAndDecode(const char * /*path*/, uint32_t &decodeMs) {
  decodeMs = 0;
  return false;
}
