#pragma once

#include <lvgl.h>
#include "../theme/Theme.h"

namespace Os2 {

/**
 * Large overlays — progress, search, emergency.
 * Sit above the shell. Never replace the shell (except emergency takeover).
 */
class OverlayManager {
 public:
  void bind(lv_obj_t *host) { host_ = host; }

  void showProgress(const char *title, int percent) {
    if (!host_) return;
    ensureChrome(title);
    if (bar_) lv_bar_set_value(bar_, percent, LV_ANIM_ON);
    if (pct_) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d%%", percent);
      lv_label_set_text(pct_, buf);
    }
  }

  void hide() {
    if (!host_) return;
    lv_obj_add_flag(host_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(host_);
    bar_ = nullptr;
    pct_ = nullptr;
    title_ = nullptr;
  }

  void showEmergency() {
    if (!host_) return;
    lv_obj_clean(host_);
    lv_obj_clear_flag(host_, LV_OBJ_FLAG_HIDDEN);

    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();

    lv_obj_set_style_bg_color(host_, lv_color_hex(c.dangerSurface), 0);
    lv_obj_set_style_bg_opa(host_, LV_OPA_COVER, 0);

    lv_obj_t *t = lv_label_create(host_);
    lv_label_set_text(t, "EMERGENCY STOP");
    lv_obj_set_style_text_color(t, lv_color_hex(c.statusCritical), 0);
    lv_obj_set_style_text_font(t, th.type().title, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t *s = lv_label_create(host_);
    lv_label_set_text(s, "System Locked\nReset Required");
    lv_obj_set_style_text_color(s, lv_color_hex(c.dangerText), 0);
    lv_obj_set_style_text_font(s, th.type().body, 0);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s, LV_ALIGN_CENTER, 0, 24);
  }

 private:
  lv_obj_t *host_ = nullptr;
  lv_obj_t *bar_ = nullptr;
  lv_obj_t *pct_ = nullptr;
  lv_obj_t *title_ = nullptr;

  void ensureChrome(const char *title) {
    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    const Theme::Spacing &sp = th.space();

    if (title_ && bar_ && pct_) {
      if (title) lv_label_set_text(title_, title);
      return;
    }

    lv_obj_clean(host_);
    lv_obj_clear_flag(host_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(host_, lv_color_hex(c.surfaceOverlay), 0);
    lv_obj_set_style_bg_opa(host_, LV_OPA_90, 0);

    lv_obj_t *panel = lv_obj_create(host_);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &th.styleSurfaceRaised, 0);
    lv_obj_set_size(panel, 420, 160);
    lv_obj_center(panel);

    title_ = lv_label_create(panel);
    lv_label_set_text(title_, title ? title : "");
    lv_obj_add_style(title_, &th.styleTitle, 0);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, sp.pad);

    bar_ = lv_bar_create(panel);
    lv_obj_set_size(bar_, 360, 12);
    lv_obj_align(bar_, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(bar_, 0, 100);

    pct_ = lv_label_create(panel);
    lv_label_set_text(pct_, "0%");
    lv_obj_add_style(pct_, &th.styleCaption, 0);
    lv_obj_align(pct_, LV_ALIGN_BOTTOM_MID, 0, -sp.pad);
  }
};

}  // namespace Os2