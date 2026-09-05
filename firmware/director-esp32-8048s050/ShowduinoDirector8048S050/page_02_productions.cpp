#include "page_02_productions.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

/* ================================================================
 * Page 02 geometry — 800×480, below status bar, above dock
 * ================================================================ */
static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderH = OS_TITLE_H;
static const int16_t kListX = 20;
static const int16_t kListY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
static const int16_t kListW = 760;
static const int16_t kBtnH = OS_BTN_H;
static const int16_t kActionY = (int16_t)(OS_DOCK_Y - kBtnH - OS_GAP);
static const int16_t kListH = (int16_t)(kActionY - kListY - OS_GAP);
static const int16_t kRowH = 72;
static const int16_t kRowGap = OS_GAP;

static const int16_t kBtnW = 170;
static const int16_t kBtnGap = 12;

/* ---- Local view-model (fixed capacity, no heap churn) ---- */
static Page02ProductionEntry s_entries[PAGE02_MAX_PRODUCTIONS];
static int s_count = 0;
static int s_selected = -1;

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
static lv_obj_t *s_btn_load = nullptr;
static lv_obj_t *s_btn_run = nullptr;
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
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(row, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(row, selected ? 2 : 1, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(row, OS_PANEL_RADIUS, 0);
  const lv_color_t accent = showduino_theme_get_accent();
  const lv_color_t idle = lv_color_hex(ShowduinoPalette::AccentDark);
  lv_obj_set_style_border_color(row, selected ? accent : idle, 0);
}

static void update_summary_and_actions(void) {
  if (s_count_label) {
    char buf[40];
    snprintf(buf, sizeof(buf), "Productions: %d", s_count);
    lv_label_set_text(s_count_label, buf);
  }
  if (s_selected_label) {
    if (s_selected >= 0 && s_selected < s_count && s_entries[s_selected].used) {
      char buf[96];
      snprintf(buf, sizeof(buf), "%s  ·  %d on SD", s_entries[s_selected].name, s_count);
      lv_label_set_text(s_selected_label, buf);
    } else {
      char buf[48];
      snprintf(buf, sizeof(buf), "No selection  ·  %d on SD", s_count);
      lv_label_set_text(s_selected_label, buf);
    }
  }

  const bool has = (s_selected >= 0 && s_selected < s_count && s_entries[s_selected].used);
  lv_obj_t *gated[] = { s_btn_open, s_btn_load, s_btn_run };
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
      Serial.printf("[Page02] Open details: %s\n", s_entries[s_selected].name);
    }
    emit(PAGE02_CMD_OPEN);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_LOAD) == 0) {
    Serial.println("[Page02] Load → Stage");
    emit(PAGE02_CMD_LOAD);
    return;
  }
  if (strcmp(cmd, PAGE02_CMD_RUN) == 0) {
    Serial.println("[Page02] Run → Stage");
    emit(PAGE02_CMD_RUN);
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
  lv_obj_set_pos(s_header_accent, 110, 34);
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
  lv_obj_set_pos(s_selected_label, 360, 8);
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
  s_btn_load = make_action(parent, "LOAD", x, PAGE02_CMD_LOAD, false);
  x = (int16_t)(x + kBtnW + kBtnGap);
  s_btn_run = make_action(parent, "RUN", x, PAGE02_CMD_RUN, false);
  x = (int16_t)(x + kBtnW + kBtnGap);
  s_btn_open = make_action(parent, "DETAILS", x, PAGE02_CMD_OPEN, false);
}

static void build_footer(lv_obj_t *parent) {
  (void)parent;
  s_count_label = nullptr;
  s_storage_label = nullptr;
  s_notify_label = s_selected_label;
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
  memset(s_entries, 0, sizeof(s_entries));
  s_count = 0;
  s_selected = -1;

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
  showduino_theme_unregister(s_btn_load);
  showduino_theme_unregister(s_btn_run);
  for (int i = 0; i < PAGE02_MAX_PRODUCTIONS; i++) {
    showduino_theme_unregister(s_rows[i]);
  }
  if (s_root) lv_obj_clean(s_root);

  s_root = nullptr;
  s_header = s_btn_back = s_title = s_selected_label = s_header_accent = nullptr;
  s_list = s_empty = nullptr;
  s_btn_open = s_btn_load = s_btn_run = nullptr;
  s_count_label = s_storage_label = s_notify_label = nullptr;
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
  lv_obj_t *btns[] = { s_btn_back, s_btn_open, s_btn_load, s_btn_run };
  for (uint8_t i = 0; i < 4; i++) {
    if (!btns[i]) continue;
    lv_obj_set_style_border_color(btns[i], accent, 0);
    lv_obj_set_style_border_color(btns[i], accent, LV_STATE_PRESSED);
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

void page_02_productions_set_entries(const Page02ProductionEntry *entries, int count) {
  memset(s_entries, 0, sizeof(s_entries));
  s_count = 0;
  s_selected = -1;
  if (entries && count > 0) {
    if (count > PAGE02_MAX_PRODUCTIONS) count = PAGE02_MAX_PRODUCTIONS;
    for (int i = 0; i < count; i++) {
      s_entries[i] = entries[i];
      s_entries[i].used = true;
    }
    s_count = count;
    s_selected = 0;
  }
  if (s_storage_label) lv_label_set_text(s_storage_label, "Storage: SD LIBRARY");
  refresh_row_styles();
}