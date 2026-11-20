#ifndef SHOWDUINO_DIRECTOR_V11_CARD_LVGL_H
#define SHOWDUINO_DIRECTOR_V11_CARD_LVGL_H

// LVGL 9 prototype to trial on a separate Director experiment build.
// This creates a lightweight 2-label read-only status card. It does not
// register events, dispatch commands, start timers, allocate image buffers,
// or alter global LVGL styles. Call update() from the LVGL/UI task only.

#include <lvgl.h>
#include <cstring>
#include "DirectorV11CardModel.h"

namespace ShowduinoV11 {

struct CardHandle {
  lv_obj_t *root = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *state = nullptr;
  lv_obj_t *rail = nullptr;
};

inline CardHandle createCard(lv_obj_t *parent, const char *name,
                             int32_t x, int32_t y, int32_t width,
                             int32_t height = 112) {
  CardHandle c;
  if (!parent || width < 96 || height < 72) return c;

  c.root = lv_obj_create(parent);
  lv_obj_remove_style_all(c.root);
  lv_obj_set_pos(c.root, x, y);
  lv_obj_set_size(c.root, width, height);
  lv_obj_set_style_bg_color(c.root, lv_color_hex(0x171D26), 0);
  lv_obj_set_style_bg_opa(c.root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c.root, 1, 0);
  lv_obj_set_style_border_color(c.root, lv_color_hex(0x354252), 0);
  lv_obj_set_style_radius(c.root, 8, 0);
  lv_obj_clear_flag(c.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(c.root, LV_OBJ_FLAG_CLICKABLE);

  c.rail = lv_obj_create(c.root);
  lv_obj_remove_style_all(c.rail);
  lv_obj_set_pos(c.rail, 0, 0);
  lv_obj_set_size(c.rail, 4, height);
  lv_obj_set_style_bg_opa(c.rail, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(c.rail, lv_color_hex(0xA7A7A7), 0);
  lv_obj_clear_flag(c.rail, LV_OBJ_FLAG_CLICKABLE);

  c.title = lv_label_create(c.root);
  lv_label_set_text(c.title, name ? name : "NODE");
  lv_obj_set_pos(c.title, 14, 16);
  lv_obj_set_width(c.title, width - 24);
  lv_label_set_long_mode(c.title, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(c.title, lv_color_hex(0xEAF0F6), 0);

  c.state = lv_label_create(c.root);
  lv_obj_set_pos(c.state, 14, 52);
  lv_obj_set_width(c.state, width - 24);
  lv_label_set_long_mode(c.state, LV_LABEL_LONG_DOT);
  lv_label_set_text(c.state, statusLabel(CardStatus::AwaitingState));
  lv_obj_set_style_text_color(
      c.state, lv_color_hex(statusColorRgb(CardStatus::AwaitingState)), 0);
  return c;
}

inline void updateCard(CardHandle &card, const NodeEvidence &e,
                       uint32_t staleAfterMs = 6000) {
  if (!card.root) return;
  const CardStatus state = classifyNode(e, staleAfterMs);
  const lv_color_t color = lv_color_hex(statusColorRgb(state));
  if (card.state) {
    // Avoid changing LVGL object text on each incoming identical status.
    const char *label = statusLabel(state);
    if (std::strcmp(lv_label_get_text(card.state), label) != 0) {
      lv_label_set_text(card.state, label);
    }
    lv_obj_set_style_text_color(card.state, color, 0);
  }
  if (card.rail) lv_obj_set_style_bg_color(card.rail, color, 0);
}

inline void destroyCard(CardHandle &card) {
  if (card.root) lv_obj_delete(card.root);
  card = CardHandle{};
}

}  // namespace ShowduinoV11

#endif  // SHOWDUINO_DIRECTOR_V11_CARD_LVGL_H
