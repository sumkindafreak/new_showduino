#include "DisplayManager.h"
#include "DisplayPages.h"
#include "DisplaySystemPages.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace {
ShowduinoOsTheme gDisplayOs;
} // namespace


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
  gDisplayOs.begin();
  dock_ = lv_obj_create(layers_[DISPLAY_LAYER_CONTROLS]);
  lv_obj_remove_style_all(dock_);
  lv_obj_set_pos(dock_, 0, OS_DOCK_Y);
  lv_obj_set_size(dock_, DISPLAY_WIDTH, OS_DOCK_H + 10);
  lv_obj_set_style_bg_color(dock_, lv_color_hex(ShowduinoPalette::Background), 0);
  lv_obj_set_style_bg_opa(dock_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(dock_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(dock_, LV_OBJ_FLAG_CLICKABLE);

  static const char *labels[] = { "DESKTOP", "LIVE", "SHOWS", "SETTINGS", "E-STOP" };
  static const char *commands[] = {
    "SCREEN:DESKTOP", "SCREEN:LIVE", "SCREEN:SHOWS", "SCREEN:SETTINGS", "EMERGENCY:STOP"
  };
  const int gap = OS_GAP;
  const int estopW = 130;
  const int navW = (SCREEN_WIDTH - 2 * OS_MARGIN - estopW - 4 * gap) / 4;
  int x = OS_MARGIN;
  for (uint8_t i = 0; i < 5; i++) {
    const int w = (i == 4) ? estopW : navW;
    const bool danger = (i == 4);
    lv_obj_t *button = gDisplayOs.makeButton(dock_, labels[i], x, 0, w, OS_DOCK_H,
                                             dockEventThunk, this, commands[i], danger, false);
    dockButtons_[i] = button;
    x += w + gap;
  }
}

void DisplayManager::highlightDock(DisplayPageId page) {
  static const char *commands[] = {
    "SCREEN:DESKTOP", "SCREEN:LIVE", "SCREEN:SHOWS", "SCREEN:SETTINGS", "EMERGENCY:STOP"
  };
  const char *active = nullptr;
  switch (page) {
    case PAGE_DESKTOP: active = "SCREEN:DESKTOP"; break;
    case PAGE_LIVE: active = "SCREEN:LIVE"; break;
    case PAGE_SHOWS:
    case PAGE_SHOW_DETAILS: active = "SCREEN:SHOWS"; break;
    case PAGE_SETTINGS:
    case PAGE_AUDIO:
    case PAGE_LOGS: active = "SCREEN:SETTINGS"; break;
    default: active = nullptr; break;
  }
  for (uint8_t i = 0; i < 5; i++) {
    lv_obj_t *button = dockButtons_[i];
    if (!button) continue;
    const bool on = active && commands[i] && !strcmp(active, commands[i]);
    const bool danger = (i == 4);
    lv_obj_set_style_border_color(
        button,
        lv_color_hex(danger ? ShowduinoPalette::Danger
                            : (on ? ShowduinoPalette::Accent : ShowduinoPalette::AccentDark)),
        0);
    lv_obj_set_style_border_width(button, on || danger ? 2 : 1, 0);
    lv_obj_set_style_shadow_opa(button, on ? LV_OPA_40 : LV_OPA_20, 0);
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
  gDisplayOs.begin();
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
  chromeBody_ = nullptr;
  chromeStatus_ = nullptr;
  if (!chromeRoot_) return;
  lv_obj_add_flag(chromeRoot_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clean(chromeRoot_);
}

void DisplayManager::setSystemDetail(const char *text) {
  if (!chromeBody_ || !text) return;
  lv_label_set_text(chromeBody_, text);
}

void DisplayManager::refreshSystemStatus(const DisplaySnapshot &snapshot) {
  if (!chromeStatus_) return;
  char line[160];
  snprintf(line, sizeof(line), "%s    %s    %s",
           snapshot.linkState[0] ? snapshot.linkState : "LINK —",
           snapshot.safetyState[0] ? snapshot.safetyState : "SAFETY —",
           snapshot.runtimeState[0] ? snapshot.runtimeState : "—");
  ShowduinoOsTheme::setTextIfChanged(chromeStatus_, line);
  uint32_t colour = ShowduinoPalette::Muted;
  if (strstr(snapshot.safetyState, "E-STOP") || strstr(snapshot.safetyState, "FAULT") ||
      strstr(snapshot.linkState, "LOST")) {
    colour = ShowduinoPalette::Danger;
  } else if (strstr(snapshot.linkState, "SEARCH") || strstr(snapshot.linkState, "NO STAGE")) {
    colour = ShowduinoPalette::Warn;
  } else if (strstr(snapshot.linkState, "OK")) {
    colour = ShowduinoPalette::Accent;
  }
  lv_obj_set_style_text_color(chromeStatus_, lv_color_hex(colour), 0);
}

void DisplayManager::buildSystemChrome(DisplayPageId page, const DisplayPage *desc) {
  (void)desc;
  gDisplayOs.begin();
  if (!layers_[DISPLAY_LAYER_CONTROLS]) return;
  if (!chromeRoot_) {
    chromeRoot_ = lv_obj_create(layers_[DISPLAY_LAYER_CONTROLS]);
    lv_obj_remove_style_all(chromeRoot_);
    lv_obj_set_pos(chromeRoot_, 0, 0);
    lv_obj_set_size(chromeRoot_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_clear_flag(chromeRoot_, LV_OBJ_FLAG_SCROLLABLE);
  }
  lv_obj_clean(chromeRoot_);
  chromeBody_ = nullptr;
  chromeStatus_ = nullptr;
  lv_obj_set_style_bg_color(chromeRoot_, lv_color_hex(ShowduinoPalette::Background), 0);
  lv_obj_set_style_bg_opa(chromeRoot_, LV_OPA_COVER, 0);
  lv_obj_add_flag(chromeRoot_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(chromeRoot_, LV_OBJ_FLAG_HIDDEN);

  const SystemPageSpec *spec = displaySystemPageSpec(page);
  const uint32_t accent = (spec && spec->dangerAccent)
                              ? ShowduinoPalette::Danger
                              : ShowduinoPalette::Accent;
  const uint32_t accentDark = (spec && spec->dangerAccent)
                                  ? ShowduinoPalette::DangerDark
                                  : ShowduinoPalette::AccentDark;

  gDisplayOs.paintChassis(chromeRoot_, accent, accentDark);

  lv_obj_t *brand = lv_label_create(chromeRoot_);
  lv_label_set_text(brand, "SHOWDUINO");
  lv_obj_set_style_text_color(brand, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(brand, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(brand, 34, OS_BODY_Y);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *kicker = lv_label_create(chromeRoot_);
  lv_label_set_text(kicker, spec && spec->kicker ? spec->kicker : "///  DIRECTOR SYSTEM");
  lv_obj_set_style_text_color(kicker, lv_color_hex(accent), 0);
  lv_obj_set_style_text_font(kicker, &lv_font_montserrat_16, 0);
  lv_obj_align(kicker, LV_ALIGN_TOP_RIGHT, -34, OS_BODY_Y);
  lv_obj_clear_flag(kicker, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *title = lv_label_create(chromeRoot_);
  lv_label_set_text(title, spec && spec->title ? spec->title : displayPageTitle(page));
  lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_obj_set_pos(title, 34, OS_BODY_Y + 40);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *subtitle = lv_label_create(chromeRoot_);
  lv_label_set_text(subtitle, spec && spec->subtitle ? spec->subtitle : "");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(accent), 0);
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
  lv_obj_set_pos(subtitle, 36, OS_BODY_Y + 78);
  lv_obj_clear_flag(subtitle, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *infoBox = gDisplayOs.makePanel(chromeRoot_, 34, OS_BODY_Y + 118, 732, 120, false);
  lv_obj_set_style_border_color(infoBox, lv_color_hex(accentDark), 0);

  chromeBody_ = lv_label_create(infoBox);
  lv_obj_set_width(chromeBody_, 700);
  lv_label_set_long_mode(chromeBody_, LV_LABEL_LONG_WRAP);
  lv_label_set_text(chromeBody_, spec && spec->body ? spec->body : "");
  lv_obj_set_style_text_color(chromeBody_, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(chromeBody_, &lv_font_montserrat_14, 0);
  lv_obj_set_pos(chromeBody_, 12, 10);

  chromeStatus_ = lv_label_create(infoBox);
  lv_obj_set_width(chromeStatus_, 700);
  lv_label_set_long_mode(chromeStatus_, LV_LABEL_LONG_CLIP);
  lv_label_set_text(chromeStatus_, "Awaiting Stage status");
  lv_obj_set_style_text_color(chromeStatus_, lv_color_hex(ShowduinoPalette::Muted), 0);
  lv_obj_set_style_text_font(chromeStatus_, &lv_font_montserrat_12, 0);
  lv_obj_set_pos(chromeStatus_, 12, 86);

  const uint8_t n = spec ? spec->actionCount : 0;
  if (n > 0 && spec->actions) {
    const int btnH = OS_BTN_H;
    const int gap = 16;
    const int totalGap = gap * (n - 1);
    const int btnW = (732 - totalGap) / n;
    int x = 34;
    const int y = SCREEN_HEIGHT - 28 - btnH;
    for (uint8_t i = 0; i < n; i++) {
      gDisplayOs.makeButton(chromeRoot_, spec->actions[i].label, x, y, btnW, btnH,
                            dockEventThunk, this, spec->actions[i].command,
                            spec->actions[i].danger, false);
      x += btnW + gap;
    }
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
  highlightDock(page);
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
  refreshSystemStatus(snapshot);
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
