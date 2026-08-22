#include "page_02_productions.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "DisplayTypes.h"

/* ================================================================
 * Page 02 geometry — 800×480
 * ================================================================ */
static const int16_t kHeaderY = 8;
static const int16_t kHeaderH = 48;
static const int16_t kListX = 20;
static const int16_t kListY = 64;
static const int16_t kListW = 760;
static const int16_t kListH = 292;
static const int16_t kRowH = 72;
static const int16_t kRowGap = 8;
static const int16_t kActionY = 368;
static const int16_t kActionH = 48;
static const int16_t kFooterY = 428;
static const int16_t kFooterH = 44;

static const int16_t kBtnW = 170;
static const int16_t kBtnH = 44;
static const int16_t kBtnGap = 12;

/* ---- Local view-model (fixed capacity, no heap churn) ---- */
static Page02ProductionEntry s_entries[PAGE02_MAX_PRODUCTIONS];
static int s_count = 0;
static int s_selected = -1;
static uint16_t s_next_id = 1;

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_btn_back = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_selected_label = nullptr;
static lv_obj_t *s_header_accent = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_empty = nullptr;
static lv_obj_t *s_rows[PAGE02_MAX_PRODUCTIONS];
static lv_obj_t *s_row_names[PAGE02_MAX_PRODUCTIONS];
static lv_obj_t *s_row_descs[PAGE02_MAX_PRODUCTIONS];
static lv_obj_t *s_row_dates[PAGE02_MAX_PRODUCTIONS];
static lv_obj_t *s_btn_open = nullptr;
static lv_obj_t *s_btn_new = nullptr;
static lv_obj_t *s_btn_dup = nullptr;
static lv_obj_t *s_btn_del = nullptr;
static lv_obj_t *s_footer = nullptr;
static lv_obj_t *s_count_label = nullptr;
static lv_obj_t *s_storage_label = nullptr;
static lv_obj_t *s_notify_label = nullptr;
static lv_obj_t *s_dialog = nullptr;
static lv_obj_t *s_dialog_msg = nullptr;

static page02_command_fn s_command_cb = nullptr;
static bool s_active = false;

static void emit(const char *cmd) {
  if (s_command_cb != nullptr && cmd != nullptr) {
    s_command_cb(cmd);
  }
}

static void seed_defaults(void) {
  memset(s_entries, 0, sizeof(s_entries));
  s_count = 0;
  s_selected = -1;
  s_next_id = 1;

  strncpy(s_entries[0].id, "demo", sizeof(s_entries[0].id) - 1);
  strncpy(s_entries[0].name, "DEMO PRODUCTION", sizeof(s_entries[0].name) - 1);
  strncpy(s_entries[0].description, "Interface test placeholder", sizeof(s_entries[0].description) - 1);
  strncpy(s_entries[0].modified, "—", sizeof(s_entries[0].modified) - 1);
  s_entries[0].used = true;

  strncpy(s_entries[1].id, "untitled", sizeof(s_entries[1].id) - 1);
  strncpy(s_entries[1].name, "UNTITLED SHOW", sizeof(s_entries[1].name) - 1);
  strncpy(s_entries[1].description, "Empty local placeholder", sizeof(s_entries[1].description) - 1);
  strncpy(s_entries[1].modified, "—", sizeof(s_entries[1].modified) - 1);
  s_entries[1].used = true;

  s_count = 2;
  s_selected = 0;
  s_next_id = 3;
}

static void style_action_button(lv_obj_t *btn, bool danger) {
  lv_obj_remove_style_all(btn);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn,
      lv_color_hex(danger ? ShowduinoPalette::DangerPanel : ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn,
      lv_color_hex(danger ? ShowduinoPalette::DangerDark : ShowduinoPalette::AccentDim),
      LV_STATE_PRESSED);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
}

static void style_row(lv_obj_t *row, bool selected) {
  lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(row, selected ? 3 : 2, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, 10, 0);
  const lv_color_t accent = showduino_theme_get_accent();
  const lv_color_t idle = lv_color_hex(ShowduinoPalette::AccentDark);
  lv_obj_set_style_border_color(row, selected ? accent : idle, 0);
}

static void update_summary_and_actions(void) {
  if (s_selected_label) {
    if (s_selected >= 0 && s_selected < s_count && s_entries[s_selected].used) {
      char buf[96];
      snprintf(buf, sizeof(buf), "Selected: %s", s_entries[s_selected].name);
      lv_label_set_text(s_selected_label, buf);
    } else {
      lv_label_set_text(s_selected_label, "Selected: —");
    }
  }

  const bool has = (s_selected >= 0 && s_selected < s_count && s_entries[s_selected].used);
  lv_obj_t *gated[] = { s_btn_open, s_btn_dup, s_btn_del };
  for (uint8_t i = 0; i < 3; i++) {
    if (!gated[i]) continue;
    if (has) {
      lv_obj_clear_state(gated[i], LV_STATE_DISABLED);
      lv_obj_set_style_opa(gated[i], LV_OPA_COVER, 0);
    } else {
      lv_obj_add_state(gated[i], LV_STATE_DISABLED);
      lv_obj_set_style_opa(gated[i], LV_OPA_40, 0);
    }
  }

  if (s_count_label) {
    char buf[40];
    snprintf(buf, sizeof(buf), "Productions: %d", s_count);
    lv_label_set_text(s_count_label, buf);
  }
  if (s_empty) {
    if (s_count <= 0) lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
  }
}

static void refresh_row_styles(void) {
  for (int i = 0; i < PAGE02_MAX_PRODUCTIONS; i++) {
    if (!s_rows[i]) continue;
    if (i < s_count && s_entries[i].used) {
      lv_obj_clear_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
      style_row(s_rows[i], i == s_selected);
      if (s_row_names[i]) lv_label_set_text(s_row_names[i], s_entries[i].name);
      if (s_row_descs[i]) lv_label_set_text(s_row_descs[i], s_entries[i].description);
      if (s_row_dates[i]) {
        char dbuf[40];
        snprintf(dbuf, sizeof(dbuf), "Modified: %s", s_entries[i].modified);
        lv_label_set_text(s_row_dates[i], dbuf);
      }
      const int16_t y = (int16_t)(i * (kRowH + kRowGap));
      lv_obj_set_pos(s_rows[i], 0, y);
    } else {
      lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (s_list) {
    const int16_t content_h = (int16_t)(s_count * (kRowH + kRowGap) + 8);
    lv_obj_set_style_min_height(s_list, content_h > kListH ? content_h : kListH, 0);
  }
  update_summary_and_actions();
}

static void select_index(int index) {
  if (index < 0 || index >= s_count || !s_entries[index].used) return;
  s_selected = index;
  Serial.printf("[Page02] Selected production: %s\n", s_entries[index].name);
  refresh_row_styles();
}

static void close_dialog(void) {
  if (s_dialog) {
    showduino_theme_unregister(s_dialog);
    lv_obj_delete(s_dialog);
    s_dialog = nullptr;
    s_dialog_msg = nullptr;
  }
}

static void dialog_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const char *tag = (const char *)lv_event_get_user_data(e);
  if (!tag) return;
  if (strcmp(tag, "cancel") == 0) {
    Serial.println("[Page02] Delete cancelled");
    close_dialog();
    return;
  }
  if (strcmp(tag, "confirm") == 0) {
    if (s_selected >= 0 && s_selected < s_count && s_entries[s_selected].used) {
      Serial.printf("[Page02] Delete confirmed: %s\n", s_entries[s_selected].name);
      /* Local model only — no SD destructive ops. */
      for (int i = s_selected; i + 1 < s_count; i++) {
        s_entries[i] = s_entries[i + 1];
      }
      memset(&s_entries[s_count - 1], 0, sizeof(s_entries[0]));
      s_count--;
      if (s_selected >= s_count) s_selected = s_count - 1;
      if (s_notify_label) lv_label_set_text(s_notify_label, "Deleted (local)");
    }
    close_dialog();
    refresh_row_styles();
  }
}

static void open_delete_dialog(void) {
  if (!s_root || s_selected < 0 || s_selected >= s_count) return;
  close_dialog();

  s_dialog = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_dialog);
  lv_obj_set_size(s_dialog, 420, 180);
  lv_obj_center(s_dialog);
  lv_obj_set_style_bg_color(s_dialog, lv_color_hex(ShowduinoPalette::Panel), 0);
  lv_obj_set_style_bg_opa(s_dialog, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_dialog, 2, 0);
  lv_obj_set_style_radius(s_dialog, 10, 0);
  lv_obj_clear_flag(s_dialog, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(s_dialog, SHOWDUINO_THEME_ROLE_BORDER);

  lv_obj_t *title = lv_label_create(s_dialog);
  lv_label_set_text(title, "DELETE PRODUCTION?");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_pos(title, 20, 16);

  s_dialog_msg = lv_label_create(s_dialog);
  char msg[96];
  snprintf(msg, sizeof(msg), "Remove \"%s\" from the local list?",
           s_entries[s_selected].name);
  lv_label_set_text(s_dialog_msg, msg);
  lv_obj_set_width(s_dialog_msg, 380);
  lv_label_set_long_mode(s_dialog_msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_dialog_msg, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_dialog_msg, lv_color_hex(ShowduinoPalette::Muted), 0);
  lv_obj_set_pos(s_dialog_msg, 20, 52);

  lv_obj_t *cancel = lv_button_create(s_dialog);
  style_action_button(cancel, false);
  lv_obj_set_size(cancel, 150, 40);
  lv_obj_set_pos(cancel, 40, 120);
  lv_obj_add_event_cb(cancel, dialog_event, LV_EVENT_CLICKED, (void *)"cancel");
  lv_obj_t *cl = lv_label_create(cancel);
  lv_label_set_text(cl, "CANCEL");
  lv_obj_set_style_text_color(cl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(cl);

  lv_obj_t *ok = lv_button_create(s_dialog);
  style_action_button(ok, true);
  lv_obj_set_size(ok, 150, 40);
  lv_obj_set_pos(ok, 230, 120);
  lv_obj_set_style_border_color(ok, lv_color_hex(ShowduinoPalette::Danger), 0);
  lv_obj_add_event_cb(ok, dialog_event, LV_EVENT_CLICKED, (void *)"confirm");
  lv_obj_t *ol = lv_label_create(ok);
  lv_label_set_text(ol, "DELETE");
  lv_obj_set_style_text_color(ol, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(ol);
}

static void row_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const int index = (int)(intptr_t)lv_event_get_user_data(e);
  select_index(index);
}

static void action_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *t = (lv_obj_t *)lv_event_get_target(e);
  if (!t || lv_obj_has_state(t, LV_STATE_DISABLED)) return;
  const char *cmd = (const char *)lv_obj_get_user_data(t);
  if (!cmd) return;

  if (strcmp(cmd, PAGE02_CMD_BACK) == 0) {
    Serial.println("[Page02] Back → Home");
    close_dialog();
    emit(PAGE02_CMD_BACK);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_OPEN) == 0) {
    if (s_selected >= 0) {
      Serial.printf("[Page02] Open requested: %s\n", s_entries[s_selected].name);
    }
    emit(PAGE02_CMD_OPEN);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_NEW) == 0) {
    Serial.println("[Page02] New production requested");
    if (s_count >= PAGE02_MAX_PRODUCTIONS) {
      if (s_notify_label) lv_label_set_text(s_notify_label, "List full");
      emit(PAGE02_CMD_NEW);
      return;
    }
    Page02ProductionEntry &e = s_entries[s_count];
    memset(&e, 0, sizeof(e));
    snprintf(e.id, sizeof(e.id), "local_%u", (unsigned)s_next_id++);
    snprintf(e.name, sizeof(e.name), "NEW PRODUCTION %u", (unsigned)(s_count + 1));
    strncpy(e.description, "Created locally (not saved to SD)", sizeof(e.description) - 1);
    strncpy(e.modified, "local", sizeof(e.modified) - 1);
    e.used = true;
    s_selected = s_count;
    s_count++;
    if (s_notify_label) lv_label_set_text(s_notify_label, "Created (local)");
    refresh_row_styles();
    emit(PAGE02_CMD_NEW);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_DUPLICATE) == 0) {
    if (s_selected < 0 || s_count >= PAGE02_MAX_PRODUCTIONS) {
      emit(PAGE02_CMD_DUPLICATE);
      return;
    }
    Page02ProductionEntry &src = s_entries[s_selected];
    Page02ProductionEntry &dst = s_entries[s_count];
    dst = src;
    snprintf(dst.id, sizeof(dst.id), "dup_%u", (unsigned)s_next_id++);
    char nbuf[48];
    snprintf(nbuf, sizeof(nbuf), "%s COPY", src.name);
    strncpy(dst.name, nbuf, sizeof(dst.name) - 1);
    dst.name[sizeof(dst.name) - 1] = '\0';
    strncpy(dst.modified, "local", sizeof(dst.modified) - 1);
    dst.used = true;
    s_selected = s_count;
    s_count++;
    Serial.printf("[Page02] Duplicated → %s\n", dst.name);
    if (s_notify_label) lv_label_set_text(s_notify_label, "Duplicated (local)");
    refresh_row_styles();
    emit(PAGE02_CMD_DUPLICATE);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_DELETE) == 0) {
    open_delete_dialog();
    return;
  }
}

static lv_obj_t *make_action(lv_obj_t *parent, const char *label, int16_t x,
                             const char *cmd, bool danger) {
  lv_obj_t *btn = lv_button_create(parent);
  style_action_button(btn, danger);
  lv_obj_set_pos(btn, x, kActionY);
  lv_obj_set_size(btn, kBtnW, kBtnH);
  lv_obj_set_user_data(btn, (void *)cmd);
  lv_obj_add_event_cb(btn, action_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(lab);
  return btn;
}

static void build_header(lv_obj_t *parent) {
  s_header = lv_obj_create(parent);
  lv_obj_remove_style_all(s_header);
  lv_obj_set_pos(s_header, 0, kHeaderY);
  lv_obj_set_size(s_header, DISPLAY_WIDTH, kHeaderH);
  lv_obj_set_style_bg_opa(s_header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

  s_header_accent = lv_obj_create(s_header);
  lv_obj_remove_style_all(s_header_accent);
  lv_obj_set_pos(s_header_accent, 110, 42);
  lv_obj_set_size(s_header_accent, 120, 3);
  lv_obj_set_style_bg_opa(s_header_accent, LV_OPA_COVER, 0);
  showduino_theme_register(s_header_accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s_btn_back = lv_button_create(s_header);
  style_action_button(s_btn_back, false);
  lv_obj_set_pos(s_btn_back, 12, 4);
  lv_obj_set_size(s_btn_back, 88, 40);
  lv_obj_set_user_data(s_btn_back, (void *)PAGE02_CMD_BACK);
  lv_obj_add_event_cb(s_btn_back, action_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(s_btn_back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(bl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(bl);

  s_title = lv_label_create(s_header);
  lv_label_set_text(s_title, "PRODUCTIONS");
  lv_obj_set_pos(s_title, 110, 10);
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_16, 0);
  showduino_theme_register(s_title, SHOWDUINO_THEME_ROLE_TEXT);

  s_selected_label = lv_label_create(s_header);
  lv_label_set_text(s_selected_label, "Selected: —");
  lv_obj_set_pos(s_selected_label, 360, 14);
  lv_obj_set_width(s_selected_label, 420);
  lv_label_set_long_mode(s_selected_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_selected_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_selected_label, lv_color_hex(ShowduinoPalette::Muted), 0);
}

static void build_list(lv_obj_t *parent) {
  s_list = lv_obj_create(parent);
  lv_obj_remove_style_all(s_list);
  lv_obj_set_pos(s_list, kListX, kListY);
  lv_obj_set_size(s_list, kListW, kListH);
  lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
  lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
  lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

  s_empty = lv_label_create(s_list);
  lv_label_set_text(s_empty, "NO PRODUCTIONS FOUND");
  lv_obj_set_style_text_font(s_empty, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s_empty, lv_color_hex(ShowduinoPalette::Muted), 0);
  lv_obj_align(s_empty, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < PAGE02_MAX_PRODUCTIONS; i++) {
    s_rows[i] = lv_obj_create(s_list);
    lv_obj_remove_style_all(s_rows[i]);
    lv_obj_set_size(s_rows[i], kListW - 8, kRowH);
    lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_rows[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_rows[i], row_event, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    showduino_theme_register(s_rows[i], SHOWDUINO_THEME_ROLE_BORDER);
    lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);

    s_row_names[i] = lv_label_create(s_rows[i]);
    lv_obj_set_pos(s_row_names[i], 16, 10);
    lv_obj_set_width(s_row_names[i], kListW - 40);
    lv_label_set_long_mode(s_row_names[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(s_row_names[i], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_row_names[i], lv_color_hex(ShowduinoPalette::Text), 0);

    s_row_descs[i] = lv_label_create(s_rows[i]);
    lv_obj_set_pos(s_row_descs[i], 16, 34);
    lv_obj_set_width(s_row_descs[i], kListW - 200);
    lv_label_set_long_mode(s_row_descs[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(s_row_descs[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_row_descs[i], lv_color_hex(ShowduinoPalette::Muted), 0);

    s_row_dates[i] = lv_label_create(s_rows[i]);
    lv_obj_set_pos(s_row_dates[i], kListW - 180, 34);
    lv_obj_set_width(s_row_dates[i], 160);
    lv_label_set_long_mode(s_row_dates[i], LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(s_row_dates[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_row_dates[i], lv_color_hex(ShowduinoPalette::Muted), 0);
  }
}

static void build_actions(lv_obj_t *parent) {
  int16_t x = 20;
  s_btn_open = make_action(parent, "OPEN", x, PAGE02_CMD_OPEN, false);
  x = (int16_t)(x + kBtnW + kBtnGap);
  s_btn_new = make_action(parent, "NEW", x, PAGE02_CMD_NEW, false);
  x = (int16_t)(x + kBtnW + kBtnGap);
  s_btn_dup = make_action(parent, "DUPLICATE", x, PAGE02_CMD_DUPLICATE, false);
  x = (int16_t)(x + kBtnW + kBtnGap);
  s_btn_del = make_action(parent, "DELETE", x, PAGE02_CMD_DELETE, true);
}

static void build_footer(lv_obj_t *parent) {
  s_footer = lv_obj_create(parent);
  lv_obj_remove_style_all(s_footer);
  lv_obj_set_pos(s_footer, 16, kFooterY);
  lv_obj_set_size(s_footer, 768, kFooterH);
  lv_obj_set_style_bg_opa(s_footer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_footer, LV_OBJ_FLAG_SCROLLABLE);

  s_count_label = lv_label_create(s_footer);
  lv_label_set_text(s_count_label, "Productions: 0");
  lv_obj_set_pos(s_count_label, 8, 12);
  lv_obj_set_style_text_font(s_count_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_count_label, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_storage_label = lv_label_create(s_footer);
  lv_label_set_text(s_storage_label, "Storage: LOCAL MODEL");
  lv_obj_set_pos(s_storage_label, 200, 12);
  lv_obj_set_style_text_font(s_storage_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_storage_label, lv_color_hex(ShowduinoPalette::Muted), 0);

  s_notify_label = lv_label_create(s_footer);
  lv_label_set_text(s_notify_label, "Notify: —");
  lv_obj_set_pos(s_notify_label, 480, 12);
  lv_obj_set_width(s_notify_label, 270);
  lv_label_set_long_mode(s_notify_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(s_notify_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_notify_label, lv_color_hex(ShowduinoPalette::Muted), 0);
}

void page_02_productions_create(lv_obj_t *parent, page02_command_fn command_cb) {
  if (!parent) {
    Serial.println("[Page02] create failed — parent null");
    return;
  }
  if (s_active) page_02_productions_destroy();

  showduino_theme_init();
  s_command_cb = command_cb;
  s_root = parent;
  seed_defaults();

  Serial.println("[Page02] creating Productions page…");
  build_header(parent);
  build_list(parent);
  build_actions(parent);
  build_footer(parent);
  refresh_row_styles();
  page_02_productions_apply_theme();

  s_active = true;
  Serial.println("[Page02] Productions page ready");
}

void page_02_productions_destroy(void) {
  if (!s_active && !s_root) return;
  Serial.println("[Page02] destroying Productions page");
  close_dialog();
  /* Unregister only this page's objects — do not wipe Page 01 theme entries. */
  showduino_theme_unregister(s_header_accent);
  showduino_theme_unregister(s_title);
  showduino_theme_unregister(s_btn_back);
  showduino_theme_unregister(s_btn_open);
  showduino_theme_unregister(s_btn_new);
  showduino_theme_unregister(s_btn_dup);
  showduino_theme_unregister(s_btn_del);
  for (int i = 0; i < PAGE02_MAX_PRODUCTIONS; i++) {
    showduino_theme_unregister(s_rows[i]);
  }
  if (s_root) lv_obj_clean(s_root);

  s_root = nullptr;
  s_header = s_btn_back = s_title = s_selected_label = s_header_accent = nullptr;
  s_list = s_empty = nullptr;
  s_btn_open = s_btn_new = s_btn_dup = s_btn_del = nullptr;
  s_footer = s_count_label = s_storage_label = s_notify_label = nullptr;
  memset(s_rows, 0, sizeof(s_rows));
  memset(s_row_names, 0, sizeof(s_row_names));
  memset(s_row_descs, 0, sizeof(s_row_descs));
  memset(s_row_dates, 0, sizeof(s_row_dates));
  s_command_cb = nullptr;
  s_active = false;
}

bool page_02_productions_is_active(void) { return s_active; }

void page_02_productions_apply_theme(void) {
  showduino_theme_apply();
  refresh_row_styles();
  const lv_color_t accent = showduino_theme_get_accent();
  lv_obj_t *btns[] = { s_btn_back, s_btn_open, s_btn_new, s_btn_dup };
  for (uint8_t i = 0; i < 4; i++) {
    if (!btns[i]) continue;
    lv_obj_set_style_border_color(btns[i], accent, 0);
    lv_obj_set_style_border_color(btns[i], accent, LV_STATE_PRESSED);
  }
  if (s_btn_del) {
    lv_obj_set_style_border_color(s_btn_del, lv_color_hex(ShowduinoPalette::Danger), 0);
  }
  if (s_dialog) {
    lv_obj_set_style_border_color(s_dialog, accent, 0);
  }
}

const char *page_02_productions_selected_id(void) {
  if (s_selected < 0 || s_selected >= s_count) return "";
  return s_entries[s_selected].id;
}

const char *page_02_productions_selected_name(void) {
  if (s_selected < 0 || s_selected >= s_count) return "";
  return s_entries[s_selected].name;
}

int page_02_productions_count(void) { return s_count; }