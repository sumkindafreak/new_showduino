#include "DisplayManager.h"
#include "DisplayPages.h"
#include "ShowduinoOsPalette.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

void DisplayManager::probeCapabilities() {
  caps_.psram = (psramFound() && heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0);
  caps_.bmp = false;
  caps_.png = false;
  caps_.jpeg = false;
  caps_.animations = false;
}

const char *DisplayManager::commandButtonLabel(const char *command) {
  if (!command || !command[0]) return "OK";
  if (!strcmp(command, "SCREEN:DESKTOP")) return "DESK";
  if (!strcmp(command, "SCREEN:NODES")) return "NODES";
  if (!strcmp(command, "SCREEN:SHOWS")) return "SHOWS";
  if (!strcmp(command, "SCREEN:SETTINGS")) return "SETTINGS";
  if (!strcmp(command, "SCREEN:LOGS")) return "LOGS";
  if (!strcmp(command, "UI:LOCK:UNLOCK")) return "UNLOCK";
  if (!strcmp(command, "UI:LOCK:CONFIRM")) return "CONFIRM";
  if (!strcmp(command, "UI:LOCK:CANCEL")) return "CANCEL";
  if (!strcmp(command, "UI:NET:RETRY")) return "RETRY";
  if (!strcmp(command, "UI:NET:SCAN")) return "SCAN";
  if (!strcmp(command, "UI:SYSTEM:REBOOT")) return "REBOOT";
  if (!strcmp(command, "UI:DISCOVERY:SCAN")) return "SCAN";
  if (!strcmp(command, "UI:COMPLETE:MENU")) return "MENU";
  if (!strcmp(command, "UI:COMPLETE:RUN")) return "RUN AGAIN";
  if (!strcmp(command, "UI:COMPLETE:EXPORT")) return "EXPORT";
  if (!strcmp(command, "STORAGE:REPAIR")) return "REPAIR";
  if (!strcmp(command, "STORAGE:BACKUP")) return "BACKUP";
  if (!strcmp(command, "STORAGE:EXPORT")) return "EXPORT";
  if (!strcmp(command, "STORAGE:STATUS")) return "STORAGE";
  if (!strcmp(command, "STATUS:REQUEST")) return "STATUS";
  if (!strcmp(command, "SELFTEST:START")) return "SELF TEST";
  const char *colon = strrchr(command, ':');
  return colon && colon[1] ? colon + 1 : command;
}

lv_obj_t *DisplayManager::layer(DisplayLayerId id) const {
  return id < DISPLAY_LAYER_COUNT ? layers_[id] : nullptr;
}

void DisplayManager::dockEventThunk(lv_event_t *event) {
  if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  DisplayManager *manager = static_cast<DisplayManager *>(lv_event_get_user_data(event));
  lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(event));
  const char *command = target ? static_cast<const char *>(lv_obj_get_user_data(target)) : nullptr;
  if (manager && manager->commandFn_ && command) manager->commandFn_(command);
}

void DisplayManager::buildDock() {
  if (dock_ || !layers_[DISPLAY_LAYER_CONTROLS]) return;
  dock_ = lv_obj_create(layers_[DISPLAY_LAYER_CONTROLS]);
  lv_obj_remove_style_all(dock_);
  lv_obj_set_pos(dock_, 0, 402);
  lv_obj_set_size(dock_, DISPLAY_WIDTH, 78);
  lv_obj_set_style_bg_color(dock_, lv_color_hex(ShowduinoPalette::Background), 0);
  lv_obj_set_style_bg_opa(dock_, LV_OPA_90, 0);
  lv_obj_clear_flag(dock_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(dock_, LV_OBJ_FLAG_CLICKABLE);

  static const char *labels[] = { "DESKTOP", "LIVE", "SHOWS", "SETTINGS", "E-STOP" };
  static const char *commands[] = {
    "SCREEN:DESKTOP", "SCREEN:LIVE", "SCREEN:SHOWS", "SCREEN:SETTINGS", "EMERGENCY:STOP"
  };
  static const int16_t widths[] = { 150, 150, 150, 150, 130 };
  int32_t x = 12;
  for (uint8_t i = 0; i < 5; i++) {
    lv_obj_t *button = lv_button_create(dock_);
    lv_obj_set_pos(button, x, 0);
    lv_obj_set_size(button, widths[i], 56);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_bg_color(
      button, lv_color_hex(i == 4 ? ShowduinoPalette::DangerPanel : ShowduinoPalette::PanelRaised), 0);
    lv_obj_set_style_border_color(
      button, lv_color_hex(i == 4 ? ShowduinoPalette::Danger : ShowduinoPalette::AccentDark), 0);
    lv_obj_set_style_border_width(button, i == 4 ? 2 : 1, 0);
    lv_obj_set_style_bg_color(
      button, lv_color_hex(i == 4 ? ShowduinoPalette::DangerDark : ShowduinoPalette::AccentDim),
      LV_STATE_PRESSED);
    lv_obj_set_style_border_color(
      button, lv_color_hex(i == 4 ? ShowduinoPalette::Danger : ShowduinoPalette::Accent),
      LV_STATE_PRESSED);
    lv_obj_set_style_shadow_color(
      button, lv_color_hex(i == 4 ? ShowduinoPalette::Danger : ShowduinoPalette::Accent), 0);
    lv_obj_set_style_shadow_width(button, 6, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_user_data(button, const_cast<char *>(commands[i]));
    lv_obj_add_event_cb(button, dockEventThunk, LV_EVENT_CLICKED, this);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, labels[i]);
    lv_obj_set_style_text_color(label, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_center(label);
    x += widths[i] + 8;
  }
}

void DisplayManager::setDockVisible(bool visible) {
  if (!dock_) return;
  if (visible) lv_obj_clear_flag(dock_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(dock_, LV_OBJ_FLAG_HIDDEN);
}

void DisplayManager::ensureShell() {
  if (screen_) return;
  screen_ = lv_obj_create(NULL);
  lv_obj_remove_style_all(screen_);
  lv_obj_set_size(screen_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(ShowduinoPalette::Background), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

  for (uint8_t i = 0; i < DISPLAY_LAYER_COUNT; i++) {
    layers_[i] = lv_obj_create(screen_);
    lv_obj_remove_style_all(layers_[i]);
    lv_obj_set_pos(layers_[i], 0, 0);
    lv_obj_set_size(layers_[i], DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_opa(layers_[i], LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(layers_[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(layers_[i], LV_OBJ_FLAG_CLICKABLE);
  }

  overlay_.begin(layers_[DISPLAY_LAYER_WIDGETS],
#if defined(SHOWDUINO_DISPLAY_STATS) && SHOWDUINO_DISPLAY_STATS
                 &stats_
#else
                 nullptr
#endif
  );
  buildDock();
  raiseLayers();
  Serial.println("[Display] permanent compositor ready (background/widgets/controls/temporary)");
}

void DisplayManager::raiseLayers() {
  for (uint8_t i = 0; i < DISPLAY_LAYER_COUNT; i++) {
    if (layers_[i]) lv_obj_move_foreground(layers_[i]);
  }
  if (dock_) lv_obj_move_foreground(dock_);
}

void DisplayManager::hidePagePanels() {
  for (uint8_t i = 0; i < PAGE_COUNT; i++) {
    if (pagePanels_[i]) lv_obj_add_flag(pagePanels_[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void DisplayManager::enterError(const char *reason) {
  state_ = DISPLAY_ERROR;
  phase2Active_ = false;
  Serial.printf("[Display] ERROR: %s\n", reason ? reason : "unknown");
}

void DisplayManager::begin() {
  if (begun_) return;
  begun_ = true;
  probeCapabilities();
  DisplayTheme::begin();
  background_.begin(
#if defined(SHOWDUINO_DISPLAY_STATS) && SHOWDUINO_DISPLAY_STATS
                    &stats_
#else
                    nullptr
#endif
  );
  ensureShell();
  state_ = DISPLAY_READY;
  Serial.printf("[Display] begin caps psram=%d bmp=%d\n", (int)caps_.psram, (int)caps_.bmp);
}

lv_obj_t *DisplayManager::createPagePanel(DisplayPageId page) {
  if (!begun_) begin();
  if (page <= PAGE_NONE || page >= PAGE_COUNT) return nullptr;
  const DisplayPage *desc = displayPageById(page);
  /* System/modal pages use LVGL chrome instead of a registered panel. */
  if (desc && !desc->hybridPanel) return nullptr;
  if (pagePanels_[page]) return pagePanels_[page];
  lv_obj_t *panel = lv_obj_create(layers_[DISPLAY_LAYER_CONTROLS]);
  lv_obj_remove_style_all(panel);
  lv_obj_set_pos(panel, 0, 0);
  lv_obj_set_size(panel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  pagePanels_[page] = panel;
  if (dock_) lv_obj_move_foreground(dock_);
  return panel;
}

bool DisplayManager::assetsReadyForPage(DisplayPageId /*page*/) const {
  return false;
}

const DisplayPage *DisplayManager::pageDesc(DisplayPageId id) const {
  return displayPageById(id);
}

bool DisplayManager::beforeShowPage(DisplayPageId /*page*/) { return true; }

void DisplayManager::afterShowPage(DisplayPageId /*page*/) {}

void DisplayManager::releasePage() {
  hidePagePanels();
  clearSystemChrome();
  touch_.clear();
  overlay_.setActiveOverlays(nullptr, 0);
  background_.unload();
  backgroundFallback_ = true;
  setDockVisible(true);
  phase2Active_ = false;
  currentPage_ = PAGE_NONE;
  if (state_ != DISPLAY_ERROR) state_ = DISPLAY_READY;
}

void DisplayManager::clearSystemChrome() {
  if (!chromeRoot_) return;
  lv_obj_add_flag(chromeRoot_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clean(chromeRoot_);
}

void DisplayManager::buildSystemChrome(DisplayPageId page, const DisplayPage *desc) {
  if (!layers_[DISPLAY_LAYER_CONTROLS]) return;
  if (!chromeRoot_) {
    chromeRoot_ = lv_obj_create(layers_[DISPLAY_LAYER_CONTROLS]);
    lv_obj_remove_style_all(chromeRoot_);
    lv_obj_set_pos(chromeRoot_, 0, 0);
    lv_obj_set_size(chromeRoot_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_clear_flag(chromeRoot_, LV_OBJ_FLAG_SCROLLABLE);
  }
  lv_obj_clean(chromeRoot_);
  lv_obj_set_style_bg_opa(chromeRoot_, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(chromeRoot_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(chromeRoot_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *frame = lv_obj_create(chromeRoot_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_pos(frame, 16, 16);
  lv_obj_set_size(frame, DISPLAY_WIDTH - 32, DISPLAY_HEIGHT - 32);
  lv_obj_set_style_bg_color(frame, lv_color_hex(ShowduinoPalette::Panel), 0);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_radius(frame, 8, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *accent = lv_obj_create(frame);
  lv_obj_remove_style_all(accent);
  lv_obj_set_pos(accent, 0, 0);
  lv_obj_set_size(accent, DISPLAY_WIDTH - 32, 4);
  lv_obj_set_style_bg_color(accent, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
  lv_obj_clear_flag(accent, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *brand = lv_label_create(frame);
  lv_label_set_text(brand, "SHOWDUINO");
  lv_obj_set_style_text_color(brand, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(brand, 20, 16);

  lv_obj_t *title = lv_label_create(frame);
  lv_label_set_text(title, displayPageTitle(page));
  lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(title, 20, 48);

  if (!desc) return;
  for (uint16_t i = 0; i < desc->regionCount; i++) {
    const TouchRegion &region = desc->regions[i];
    const int32_t x = region.bounds.x1;
    const int32_t y = region.bounds.y1;
    const int32_t w = region.bounds.x2 - region.bounds.x1;
    const int32_t h = region.bounds.y2 - region.bounds.y1;
    if (w < 8 || h < 8) continue;

    lv_obj_t *button = lv_button_create(chromeRoot_);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(ShowduinoPalette::AccentDark), 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(ShowduinoPalette::Accent), LV_STATE_PRESSED);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_user_data(button, const_cast<char *>(region.command));
    lv_obj_add_event_cb(button, dockEventThunk, LV_EVENT_CLICKED, this);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, commandButtonLabel(region.command));
    lv_obj_set_style_text_color(label, lv_color_hex(ShowduinoPalette::Text), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);
  }
}

void DisplayManager::applyLvglPage(DisplayPageId page, const DisplayPage *desc, bool hasPanel) {
  background_.unload();
  backgroundFallback_ = true;
  touch_.clear();
  setDockVisible(desc ? !desc->hideDock : true);

  if (hasPanel) {
    clearSystemChrome();
    overlay_.setActiveOverlays(nullptr, 0);
    lv_obj_clear_flag(pagePanels_[page], LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (desc) {
    overlay_.setLayoutPage(page);
    overlay_.setActiveOverlays(desc->overlays, desc->overlayCount);
    buildSystemChrome(page, desc);
  } else {
    overlay_.setActiveOverlays(nullptr, 0);
    clearSystemChrome();
  }
}

void DisplayManager::reloadTheme() {
  Serial.println("[Display] reloadTheme()");
  background_.invalidateCache();
  DisplayTheme::begin();
  if (phase2Active_ && currentPage_ != PAGE_NONE) {
    const DisplayPageId page = currentPage_;
    currentPage_ = PAGE_NONE;
    showPage(page);
  }
}

bool DisplayManager::loadBackground(DisplayPageId /*page*/) {
  background_.unload();
  backgroundFallback_ = true;
  return true;
}

bool DisplayManager::showPage(DisplayPageId page) {
  if (!begun_) begin();
  if (phase2Active_ && currentPage_ == page && lv_screen_active() == screen_) {
    return true;
  }
  if (!beforeShowPage(page)) {
    Serial.println("[Display] beforeShowPage aborted");
    return false;
  }

  const DisplayPage *desc = pageDesc(page);
  const bool hasPanel = (page > PAGE_NONE && page < PAGE_COUNT && pagePanels_[page] != nullptr);
  const bool wantsPanel = desc && desc->hybridPanel;

  if (wantsPanel && !hasPanel) {
    enterError("page panel not registered");
    return false;
  }
  if (!desc && !hasPanel) {
    enterError("page panel not registered");
    return false;
  }

  state_ = DISPLAY_TRANSITION;
  ensureShell();
  hidePagePanels();
  applyLvglPage(page, desc, hasPanel);

  currentPage_ = page;
  phase2Active_ = true;
  state_ = DISPLAY_READY;
  raiseLayers();
  if (dock_) lv_obj_move_foreground(dock_);
  if (lv_screen_active() != screen_) lv_screen_load(screen_);
  afterShowPage(page);
  Serial.printf("[Display] showPage %u lvgl ready\n", (unsigned)page);
  return true;
}

void DisplayManager::drawOverlay() {
  /* Overlays are LVGL objects updated via updateWidgets. */
}

void DisplayManager::invalidateRegion(const lv_area_t &area) {
  overlay_.invalidateRegion(area);
}

void DisplayManager::updateWidgets(const DisplaySnapshot &snapshot) {
  if (!phase2Active_ || state_ != DISPLAY_READY) return;
  overlay_.applySnapshot(snapshot);
}

bool DisplayManager::onTouch(int32_t x, int32_t y, bool pressed) {
  if (!phase2Active_ || state_ != DISPLAY_READY) return false;
  const char *cmd = touch_.onTouch(x, y, pressed);
  if (!cmd) return false;
  if (commandFn_) {
    Serial.printf("[Display] touch cmd %s\n", cmd);
    commandFn_(cmd);
  }
  return true;
}
