#pragma once

#include <lvgl.h>
#include "../OsTypes.h"
#include "../theme/Theme.h"

namespace Os2 {
namespace Cards {

struct Handle {
  lv_obj_t *root = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *subtitle = nullptr;
  lv_obj_t *bar = nullptr;
  int height = 0;
};

/**
 * The one primitive.
 * Every subsystem surface is a card. Every app. Everywhere.
 */
inline Handle create(lv_obj_t *parent, int x, int y, int w, int h,
                     const char *title, const char *subtitle,
                     StatusLevel status = StatusLevel::Inactive) {
  Theme::Engine &th = Theme::engine();
  const Theme::Colors &c = th.colors();
  const Theme::Spacing &sp = th.space();

  Handle out;
  out.height = h;

  out.root = lv_obj_create(parent);
  lv_obj_remove_style_all(out.root);
  lv_obj_add_style(out.root, &th.styleCard, 0);
  lv_obj_set_pos(out.root, x, y);
  lv_obj_set_size(out.root, w, h);
  lv_obj_clear_flag(out.root, LV_OBJ_FLAG_SCROLLABLE);

  out.bar = lv_obj_create(out.root);
  lv_obj_remove_style_all(out.bar);
  lv_obj_set_pos(out.bar, 0, 0);
  lv_obj_set_size(out.bar, 4, h);
  lv_obj_set_style_bg_color(out.bar, lv_color_hex(c.status(status)), 0);
  lv_obj_set_style_bg_opa(out.bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(out.bar, 2, 0);
  lv_obj_clear_flag(out.bar, LV_OBJ_FLAG_CLICKABLE);

  out.title = lv_label_create(out.root);
  lv_label_set_text(out.title, title ? title : "");
  lv_obj_add_style(out.title, &th.styleBody, 0);
  lv_obj_set_pos(out.title, sp.pad, sp.gapTight);

  out.subtitle = lv_label_create(out.root);
  lv_label_set_text(out.subtitle, subtitle ? subtitle : "");
  lv_obj_add_style(out.subtitle, &th.styleCaption, 0);
  lv_obj_set_style_text_color(out.subtitle, lv_color_hex(c.status(status)), 0);
  lv_obj_set_pos(out.subtitle, sp.pad, sp.gapTight + 28);

  return out;
}

inline void update(Handle &card, const char *subtitle, StatusLevel status) {
  if (!card.root) return;
  Theme::Engine &th = Theme::engine();
  const Theme::Colors &c = th.colors();
  if (card.subtitle && subtitle) lv_label_set_text(card.subtitle, subtitle);
  if (card.subtitle) {
    lv_obj_set_style_text_color(card.subtitle, lv_color_hex(c.status(status)), 0);
  }
  if (card.bar) {
    lv_obj_set_style_bg_color(card.bar, lv_color_hex(c.status(status)), 0);
  }
}

inline void updateTitle(Handle &card, const char *title) {
  if (card.title && title) lv_label_set_text(card.title, title);
}

}  // namespace Cards
}  // namespace Os2