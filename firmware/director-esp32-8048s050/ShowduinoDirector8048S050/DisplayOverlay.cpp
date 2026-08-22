#include "DisplayOverlay.h"
#include <string.h>

static constexpr uint32_t kHudCyan = 0x39E7FF;
static constexpr uint32_t kHudText = 0xD8FFF5;
static constexpr uint32_t kHudWarn = 0xFFB020;
static constexpr uint32_t kHudDanger = 0xFF4D4D;
static constexpr uint32_t kHudPanel = 0x060A0E;

static bool pageUsesDashboardCards(DisplayPageId page) {
  switch (page) {
    case PAGE_DESKTOP:
    case PAGE_LIVE:
    case PAGE_DIAGNOSTICS:
    case PAGE_NODES:
      return true;
    default:
      return false;
  }
}

static bool pageUsesModalFooter(DisplayPageId page) {
  switch (page) {
    case PAGE_LOCKED:
    case PAGE_UNLOCK:
    case PAGE_EMERGENCY:
    case PAGE_CONNECTION_LOST:
    case PAGE_NO_NETWORK:
    case PAGE_NO_SD:
    case PAGE_REBOOT:
    case PAGE_FIRMWARE_UPDATE:
    case PAGE_BACKUP:
    case PAGE_RECOVERY:
    case PAGE_DISCOVERY:
    case PAGE_COMPLETE:
      return true;
    default:
      return false;
  }
}

void DisplayOverlay::overlayGeomForPage(DisplayPageId page, OverlayId id, OverlayGeom &g) {
  g = {0, 0, 100, 20, false};
  const bool dash = pageUsesDashboardCards(page);
  const bool modal = pageUsesModalFooter(page);

  switch (id) {
    case OVERLAY_CLOCK:
      g = {652, 10, 138, 22, false};
      break;
    case OVERLAY_DATE:
      g = {modal ? 20 : 220, modal ? 452 : 48, modal ? 150 : 200, 18, false};
      break;
    case OVERLAY_SHOW:
      g = dash ? OverlayGeom{228, 118, 280, 22, false}
               : OverlayGeom{220, 48, 360, 22, false};
      break;
    case OVERLAY_STAGE:
      g = dash ? OverlayGeom{228, 142, 280, 20, true}
               : OverlayGeom{220, 72, 220, 20, false};
      break;
    case OVERLAY_LINK:
      g = dash ? OverlayGeom{228, 286, 168, 20, false}
               : OverlayGeom{410, 72, 200, 20, false};
      break;
    case OVERLAY_SAFETY:
      g = dash ? OverlayGeom{418, 286, 168, 20, false}
               : OverlayGeom{600, 72, 180, 20, false};
      break;
    case OVERLAY_NODECOUNT:
      g = dash ? OverlayGeom{608, 286, 168, 20, false}
               : OverlayGeom{220, 96, 160, 20, false};
      break;
    case OVERLAY_CUE:
      g = {page == PAGE_LIVE ? 228 : 608, page == PAGE_LIVE ? 108 : 308, 168, 20, false};
      break;
    case OVERLAY_ELAPSED:
      g = {page == PAGE_LIVE ? 400 : 608, page == PAGE_LIVE ? 108 : 328, 80, 20, false};
      break;
    case OVERLAY_REMAIN:
      g = {page == PAGE_LIVE ? 488 : 688, page == PAGE_LIVE ? 108 : 328, 80, 20, false};
      break;
    case OVERLAY_FOOTER:
      g = {16, 452, 768, 22, false};
      break;
    case OVERLAY_NOTIFICATION:
      g = dash ? OverlayGeom{608, 348, 168, 40, false}
               : OverlayGeom{24, 440, 752, 32, false};
      break;
    default:
      break;
  }
}

void DisplayOverlay::begin(lv_obj_t *parent, DisplayStats *stats) {
  parent_ = parent;
  stats_ = stats;
  ensureWidgets();
}

void DisplayOverlay::clear() {
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) {
    if (labels_[i]) { lv_obj_delete(labels_[i]); labels_[i] = nullptr; }
    if (backs_[i]) { lv_obj_delete(backs_[i]); backs_[i] = nullptr; }
    last_[i][0] = '\0';
    active_[i] = false;
  }
  if (progressBar_) { lv_obj_delete(progressBar_); progressBar_ = nullptr; }
  if (progressBack_) { lv_obj_delete(progressBack_); progressBack_ = nullptr; }
  parent_ = nullptr;
  layoutPage_ = PAGE_NONE;
  lastProgress_ = 255;
}

void DisplayOverlay::styleThemedLabel(lv_obj_t *lab, bool accent) {
  lv_obj_set_style_text_color(lab, lv_color_hex(accent ? kHudCyan : kHudText), 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_LEFT, 0);
}

void DisplayOverlay::ensureWidgets() {
  if (!parent_) return;
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) {
    if (labels_[i]) continue;
    OverlayGeom g{};
    overlayGeomForPage(layoutPage_ != PAGE_NONE ? layoutPage_ : PAGE_DESKTOP,
                       (OverlayId)i, g);
    backs_[i] = lv_obj_create(parent_);
    lv_obj_remove_style_all(backs_[i]);
    lv_obj_set_pos(backs_[i], g.x - 2, g.y - 1);
    lv_obj_set_size(backs_[i], g.w + 4, g.h + 2);
    lv_obj_set_style_bg_color(backs_[i], lv_color_hex(kHudPanel), 0);
    lv_obj_set_style_bg_opa(backs_[i], LV_OPA_70, 0);
    lv_obj_set_style_radius(backs_[i], 3, 0);
    lv_obj_clear_flag(backs_[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(backs_[i], LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lab = lv_label_create(parent_);
    lv_obj_set_pos(lab, g.x, g.y);
    lv_obj_set_width(lab, g.w);
    styleThemedLabel(lab, i == OVERLAY_CLOCK || i == OVERLAY_CUE);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_CLIP);
    lv_label_set_text(lab, "");
    lv_obj_add_flag(lab, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);
    labels_[i] = lab;
    last_[i][0] = '\0';
  }

  if (!progressBack_) {
    progressBack_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(progressBack_);
    lv_obj_set_style_bg_color(progressBack_, lv_color_hex(kHudPanel), 0);
    lv_obj_set_style_bg_opa(progressBack_, LV_OPA_70, 0);
    lv_obj_set_style_radius(progressBack_, 2, 0);
    lv_obj_clear_flag(progressBack_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(progressBack_, LV_OBJ_FLAG_HIDDEN);

    progressBar_ = lv_bar_create(parent_);
    lv_bar_set_range(progressBar_, 0, 100);
    lv_bar_set_value(progressBar_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(0x102820), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar_, lv_color_hex(kHudCyan), LV_PART_INDICATOR);
    lv_obj_set_style_radius(progressBar_, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(progressBar_, 2, LV_PART_INDICATOR);
    lv_obj_clear_flag(progressBar_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(progressBar_, LV_OBJ_FLAG_HIDDEN);
  }
}

void DisplayOverlay::setLayoutPage(DisplayPageId page) {
  layoutPage_ = page;
  applyGeometry();
}

void DisplayOverlay::applyGeometry() {
  ensureWidgets();
  const DisplayPageId page = layoutPage_ != PAGE_NONE ? layoutPage_ : PAGE_DESKTOP;
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) {
    OverlayGeom g{};
    overlayGeomForPage(page, (OverlayId)i, g);
    if (backs_[i]) {
      lv_obj_set_pos(backs_[i], g.x - 2, g.y - 1);
      lv_obj_set_size(backs_[i], g.w + 4, g.h + 2);
    }
    if (labels_[i]) {
      lv_obj_set_pos(labels_[i], g.x, g.y);
      lv_obj_set_width(labels_[i], g.w);
    }
  }

  OverlayGeom stageG{};
  overlayGeomForPage(page, OVERLAY_STAGE, stageG);
  if (progressBack_ && progressBar_) {
    if (stageG.showBar) {
      lv_obj_set_pos(progressBack_, stageG.x - 2, stageG.y + stageG.h + 2);
      lv_obj_set_size(progressBack_, stageG.w + 4, 8);
      lv_obj_set_pos(progressBar_, stageG.x, stageG.y + stageG.h + 3);
      lv_obj_set_size(progressBar_, stageG.w, 6);
    }
  }
}

void DisplayOverlay::setActiveOverlays(const OverlayId *ids, uint16_t count) {
  ensureWidgets();
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) active_[i] = false;
  if (ids) {
    for (uint16_t n = 0; n < count; n++) {
      if (ids[n] < OVERLAY_COUNT) active_[ids[n]] = true;
    }
  }
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) {
    if (!labels_[i]) continue;
    if (active_[i]) {
      lv_obj_clear_flag(labels_[i], LV_OBJ_FLAG_HIDDEN);
      if (backs_[i]) lv_obj_clear_flag(backs_[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(labels_[i], LV_OBJ_FLAG_HIDDEN);
      if (backs_[i]) lv_obj_add_flag(backs_[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  OverlayGeom stageG{};
  overlayGeomForPage(layoutPage_ != PAGE_NONE ? layoutPage_ : PAGE_DESKTOP,
                     OVERLAY_STAGE, stageG);
  const bool showBar = stageG.showBar && active_[OVERLAY_STAGE];
  if (progressBar_) {
    if (showBar) lv_obj_clear_flag(progressBar_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(progressBar_, LV_OBJ_FLAG_HIDDEN);
  }
  if (progressBack_) {
    if (showBar) lv_obj_clear_flag(progressBack_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(progressBack_, LV_OBJ_FLAG_HIDDEN);
  }
}

void DisplayOverlay::setLabelIfChanged(OverlayId id, const char *text) {
  if (id >= OVERLAY_COUNT || !labels_[id] || !active_[id]) return;
  if (!text) text = "";
  if (strncmp(last_[id], text, sizeof(last_[id]) - 1) == 0) return;
  strncpy(last_[id], text, sizeof(last_[id]) - 1);
  last_[id][sizeof(last_[id]) - 1] = '\0';
  lv_label_set_text(labels_[id], last_[id]);

  uint32_t color = kHudText;
  if (id == OVERLAY_SAFETY) {
    if (strstr(last_[id], "E-STOP") || strstr(last_[id], "FAULT")) color = kHudDanger;
    else if (strstr(last_[id], "CLEAR")) color = kHudCyan;
  } else if (id == OVERLAY_LINK) {
    if (strstr(last_[id], "LOST") || strstr(last_[id], "SEARCH")) color = kHudWarn;
    else if (strstr(last_[id], "OK")) color = kHudCyan;
  } else if (id == OVERLAY_CLOCK || id == OVERLAY_CUE) {
    color = kHudCyan;
  }
  lv_obj_set_style_text_color(labels_[id], lv_color_hex(color), 0);
  lv_obj_invalidate(labels_[id]);
  if (stats_) stats_->redrawCount++;
}

void DisplayOverlay::applySnapshot(const DisplaySnapshot &snap) {
  const uint32_t t0 = millis();
  if (snap.page != PAGE_NONE && snap.page != layoutPage_) {
    setLayoutPage(snap.page);
  }
  ensureWidgets();

  char nodeBuf[32];
  snprintf(nodeBuf, sizeof(nodeBuf), "Nodes: %u", (unsigned)snap.nodeCount);

  setLabelIfChanged(OVERLAY_CLOCK, snap.clock);
  setLabelIfChanged(OVERLAY_DATE, snap.date);
  setLabelIfChanged(OVERLAY_SHOW, snap.currentShow);
  setLabelIfChanged(OVERLAY_STAGE, snap.runtimeState);
  setLabelIfChanged(OVERLAY_LINK, snap.linkState);
  setLabelIfChanged(OVERLAY_SAFETY, snap.safetyState);
  setLabelIfChanged(OVERLAY_NODECOUNT, nodeBuf);
  setLabelIfChanged(OVERLAY_CUE, snap.cue);
  setLabelIfChanged(OVERLAY_ELAPSED, snap.elapsed);
  setLabelIfChanged(OVERLAY_REMAIN, snap.remain);
  setLabelIfChanged(OVERLAY_FOOTER, snap.footer);
  setLabelIfChanged(OVERLAY_NOTIFICATION, snap.notification);

  if (progressBar_ && active_[OVERLAY_STAGE]) {
    OverlayGeom stageG{};
    overlayGeomForPage(layoutPage_ != PAGE_NONE ? layoutPage_ : PAGE_DESKTOP,
                       OVERLAY_STAGE, stageG);
    if (stageG.showBar && snap.progressPct != lastProgress_) {
      lastProgress_ = snap.progressPct;
      lv_bar_set_value(progressBar_, snap.progressPct, LV_ANIM_OFF);
      lv_obj_invalidate(progressBar_);
    }
  }

  if (stats_) stats_->overlayUpdateMs = millis() - t0;
}

void DisplayOverlay::invalidateRegion(const lv_area_t &area) {
  for (uint8_t i = 0; i < OVERLAY_COUNT; i++) {
    if (!labels_[i] || !active_[i]) continue;
    lv_area_t oa;
    lv_obj_get_coords(labels_[i], &oa);
    if (oa.x2 < area.x1 || oa.x1 > area.x2 || oa.y2 < area.y1 || oa.y1 > area.y2) continue;
    lv_obj_invalidate(labels_[i]);
    if (backs_[i]) lv_obj_invalidate(backs_[i]);
  }
  if (progressBar_ && !lv_obj_has_flag(progressBar_, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_invalidate(progressBar_);
  }
}
