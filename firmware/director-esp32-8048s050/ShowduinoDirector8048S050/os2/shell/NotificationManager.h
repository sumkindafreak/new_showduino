#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "../theme/Theme.h"

namespace Os2 {

/**
 * Notifications — not dialogs.
 * Slide in. Three seconds. Gone.
 */
class NotificationManager {
 public:
  void bind(lv_obj_t *host) { host_ = host; }

  void show(const char *title, const char *body, bool ok = true) {
    if (!host_) return;
    lv_obj_clean(host_);
    lv_obj_clear_flag(host_, LV_OBJ_FLAG_HIDDEN);

    Theme::Engine &th = Theme::engine();
    const Theme::Colors &c = th.colors();
    const Theme::Spacing &sp = th.space();
    const Theme::Animation &an = th.anim();

    lv_obj_t *toast = lv_obj_create(host_);
    lv_obj_remove_style_all(toast);
    lv_obj_add_style(toast, &th.styleSurfaceRaised, 0);
    lv_obj_set_size(toast, 320, 64);
    lv_obj_align(toast, LV_ALIGN_TOP_MID, 0, sp.topBarH + sp.gap);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(toast);
    lv_label_set_text(t, title ? title : "");
    lv_obj_add_style(t, &th.styleBody, 0);
    lv_obj_set_pos(t, sp.pad, 8);

    lv_obj_t *b = lv_label_create(toast);
    lv_label_set_text(b, body ? body : "");
    lv_obj_add_style(b, &th.styleCaption, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(ok ? c.statusHealthy : c.statusWarning), 0);
    lv_obj_set_pos(b, sp.pad, 32);

    /* Fade in */
    lv_obj_set_style_opa(toast, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, toast);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, an.fastMs);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
      lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), (lv_opa_t)v, 0);
    });
    lv_anim_start(&a);

    dismissAtMs_ = millis() + an.notifyMs;
    active_ = true;
  }

  void tick(uint32_t nowMs) {
    if (!active_ || !host_) return;
    if ((int32_t)(nowMs - dismissAtMs_) < 0) return;
    lv_obj_add_flag(host_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(host_);
    active_ = false;
  }

 private:
  lv_obj_t *host_ = nullptr;
  uint32_t dismissAtMs_ = 0;
  bool active_ = false;
};

}  // namespace Os2