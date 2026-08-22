#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "DisplayTypes.h"
#include "DisplayTheme.h"
#include "DisplayBackground.h"
#include "DisplayTouchMap.h"
#include "DisplayOverlay.h"

typedef void (*DisplayCommandFn)(const char *command);

class DisplayManager {
public:
  void begin();
  bool showPage(DisplayPageId page);
  bool loadBackground(DisplayPageId page);
  lv_obj_t *createPagePanel(DisplayPageId page);
  void drawOverlay();
  void invalidateRegion(const lv_area_t &area);
  void updateWidgets(const DisplaySnapshot &snapshot);
  bool onTouch(int32_t x, int32_t y, bool pressed);

  DisplayPageId currentPage() const { return currentPage_; }
  DisplayState state() const { return state_; }
  bool isPhase2PageActive() const { return phase2Active_; }
  const DisplayCapabilities &capabilities() const { return caps_; }
  const DisplayStats &stats() const { return stats_; }

  bool beforeShowPage(DisplayPageId page);
  void afterShowPage(DisplayPageId page);
  void reloadTheme();

  /** Leave Phase-2 ownership when loading a legacy LVGL screen. */
  void releasePage();

  void setCommandHandler(DisplayCommandFn fn) { commandFn_ = fn; }
  bool assetsReadyForPage(DisplayPageId page) const;
  bool assetsReadyForDesktop() const { return assetsReadyForPage(PAGE_DESKTOP); }
  /** Theme BMPs are retired — Director UI is LVGL only. */
  bool usingBackgroundFallback() const { return true; }
  bool hasBackgroundImage() const { return false; }
  lv_obj_t *rootScreen() const { return screen_; }
  lv_obj_t *layer(DisplayLayerId id) const;

private:
  const DisplayPage *pageDesc(DisplayPageId id) const;
  void probeCapabilities();
  void enterError(const char *reason);
  void ensureShell();
  void buildDock();
  void setDockVisible(bool visible);
  void hidePagePanels();
  void raiseLayers();
  void applyLvglPage(DisplayPageId page, const DisplayPage *desc, bool hasPanel);
  void clearSystemChrome();
  void buildSystemChrome(DisplayPageId page, const DisplayPage *desc);
  static void dockEventThunk(lv_event_t *event);
  static const char *commandButtonLabel(const char *command);

  DisplayBackground background_;
  DisplayTouchMap touch_;
  DisplayOverlay overlay_;

  lv_obj_t *screen_ = nullptr;
  lv_obj_t *layers_[DISPLAY_LAYER_COUNT] = {};
  lv_obj_t *pagePanels_[PAGE_COUNT] = {};
  lv_obj_t *dock_ = nullptr;
  lv_obj_t *chromeRoot_ = nullptr;

  DisplayPageId currentPage_ = PAGE_NONE;
  DisplayState state_ = DISPLAY_LOADING;
  bool phase2Active_ = false;
  bool begun_ = false;
  bool backgroundFallback_ = false;

  DisplayCapabilities caps_{};
  DisplayStats stats_{};
  DisplayCommandFn commandFn_ = nullptr;
};

#endif