#include "page_lamp_node.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"

static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;

struct PageLampUi {
  lv_obj_t *root;
  lv_obj_t *btn_back;
  lv_obj_t *title;
  lv_obj_t *chip_presence;
  lv_obj_t *chip_presence_lab;
  lv_obj_t *chip_state;
  lv_obj_t *chip_state_lab;
  lv_obj_t *state_lab;
  lv_obj_t *feedback;
  lv_obj_t *btn_ignite;
  lv_obj_t *btn_extinguish;
  lv_obj_t *sensor_vals[6];
  lv_obj_t *btn_jewel;
  lv_obj_t *btn_flame;
  lv_obj_t *btn_off;
  lv_obj_t *btn_flick;
  lv_obj_t *btn_ignaud;
  lv_obj_t *btn_loop;
  lv_obj_t *btn_astop;
  lv_obj_t *btn_refresh;
};

static PageLampUi s;
static page_lamp_command_fn s_cb = nullptr;
static bool s_active = false;
static DirectorLampNodeControl s_model;

static void emit(const char *cmd) {
  if (s_cb && cmd) s_cb(cmd);
}

static void style_back(lv_obj_t *btn) {
  lv_obj_remove_style_all(btn);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
}

static lv_obj_t *make_chip(lv_obj_t *parent, int16_t x, int16_t y, lv_obj_t **labOut) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, 120, 28);
  lv_obj_set_style_radius(c, 10, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *lab = lv_label_create(c);
  lv_label_set_text(lab, "-");
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_center(lab);
  if (labOut) *labOut = lab;
  return c;
}

static void colour_chip(lv_obj_t *chip, lv_obj_t *lab, const char *text, uint32_t colour) {
  if (lab && text) lv_label_set_text(lab, text);
  if (lab) lv_obj_set_style_text_color(lab, lv_color_hex(colour), 0);
  if (chip) lv_obj_set_style_border_color(chip, lv_color_hex(colour), 0);
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *label, int16_t x, int16_t y,
                          int16_t w, int16_t h, const char *cmd, bool danger) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_radius(btn, OS_BTN_RADIUS, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(danger ? ShowduinoPalette::DangerPanel
                                                     : ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(danger ? ShowduinoPalette::Danger
                                                         : ShowduinoPalette::AccentDark), 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ShowduinoPalette::AccentDim), LV_STATE_PRESSED);
  lv_obj_set_style_opa(btn, LV_OPA_50, LV_STATE_DISABLED);
  lv_obj_set_user_data(btn, (void *)cmd);
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char *cmd = (const char *)lv_event_get_user_data(e);
    emit(cmd);
  }, LV_EVENT_CLICKED, (void *)cmd);
  lv_obj_t *lab = lv_label_create(btn);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
  lv_obj_center(lab);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
  return btn;
}

static void set_en(lv_obj_t *btn, bool on) {
  ShowduinoOsTheme::setEnabled(btn, on);
}

static uint32_t state_colour(const char *st) {
  if (!st) return ShowduinoPalette::Muted;
  if (!strcmp(st, "EMERGENCY") || !strcmp(st, "FAULT")) return ShowduinoPalette::Danger;
  if (!strcmp(st, "OFFLINE") || !strcmp(st, "--")) return ShowduinoPalette::Muted;
  if (!strcmp(st, "BURNING") || !strcmp(st, "FLARE")) return ShowduinoPalette::AccentBright;
  if (!strcmp(st, "STRIKING") || !strcmp(st, "IGNITING") ||
      !strcmp(st, "LOW_FLAME") || !strcmp(st, "UNSTABLE") ||
      !strcmp(st, "EXTINGUISHING")) {
    return ShowduinoPalette::Warn;
  }
  return ShowduinoPalette::Text;
}

static void apply_enable() {
  const bool live = s_model.online && !s_model.emergency;
  set_en(s.btn_ignite, live && s_model.sheet.ignite_enabled);
  set_en(s.btn_extinguish, live && s_model.sheet.extinguish_enabled);
  set_en(s.btn_jewel, live && s_model.sheet.jewel_enabled);
  set_en(s.btn_flame, live && s_model.sheet.flame_test_enabled);
  set_en(s.btn_off, live && s_model.sheet.off_enabled);
  set_en(s.btn_flick, live && s_model.sheet.audio_enabled);
  set_en(s.btn_ignaud, live && s_model.sheet.audio_enabled);
  set_en(s.btn_loop, live && s_model.sheet.audio_enabled);
  set_en(s.btn_astop, live && s_model.sheet.audio_enabled);
  set_en(s.btn_refresh, true);
}

static void paint() {
  if (!s_active) return;

  uint32_t presenceCol = ShowduinoPalette::Disabled;
  const char *presence = s_model.presence[0] ? s_model.presence : "OFFLINE";
  if (s_model.emergency) {
    presence = "EMERGENCY";
    presenceCol = ShowduinoPalette::Danger;
  } else if (!s_model.online) {
    presence = "OFFLINE";
    presenceCol = ShowduinoPalette::Disabled;
  } else {
    presenceCol = ShowduinoPalette::Accent;
  }
  colour_chip(s.chip_presence, s.chip_presence_lab, presence, presenceCol);

  const char *st = s_model.state[0] ? s_model.state : "--";
  colour_chip(s.chip_state, s.chip_state_lab, st, state_colour(st));
  if (s.state_lab) {
    lv_label_set_text(s.state_lab, st);
    lv_obj_set_style_text_color(s.state_lab, lv_color_hex(state_colour(st)), 0);
  }

  const char *sv[] = {
    s_model.button, s_model.blow, s_model.motion,
    s_model.mic, s_model.light, s_model.voltage
  };
  for (int i = 0; i < 6; i++) {
    if (s.sensor_vals[i]) {
      lv_label_set_text(s.sensor_vals[i], sv[i] && sv[i][0] ? sv[i] : "--");
    }
  }

  char fb[96];
  fb[0] = 0;
  if (s_model.emergency) {
    strncpy(fb, "EMERGENCY — controls locked", sizeof(fb) - 1);
  } else if (s_model.lastErrorText[0]) {
    snprintf(fb, sizeof(fb), "FAIL: %s", s_model.lastErrorText);
  } else if (s_model.pending != DIRECTOR_LAMP_PEND_NONE) {
    strncpy(fb, "PENDING…", sizeof(fb) - 1);
  } else if (s_model.feedback[0]) {
    strncpy(fb, s_model.feedback, sizeof(fb) - 1);
  } else if (s_model.audio[0] && strcmp(s_model.audio, "--") != 0) {
    snprintf(fb, sizeof(fb), "Audio: %s", s_model.audio);
  }
  if (s.feedback) lv_label_set_text(s.feedback, fb[0] ? fb : " ");
  apply_enable();
}

void page_lamp_node_create(lv_obj_t *parent, page_lamp_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_lamp_node_destroy();
  memset(&s, 0, sizeof(s));
  s_cb = command_cb;
  s_active = true;
  director_lamp_node_clear(&s_model);

  s.root = parent;
  lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);

  s.btn_back = make_btn(parent, "< Nodes", 16, kHeaderY, 110, 36, PAGE_LAMP_CMD_BACK, false);
  style_back(s.btn_back);

  s.title = lv_label_create(parent);
  lv_label_set_text(s.title, "LAMP NODE");
  lv_obj_set_pos(s.title, 140, kHeaderY + 6);
  lv_obj_set_style_text_font(s.title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(s.title, lv_color_hex(ShowduinoPalette::Text), 0);

  s.chip_presence = make_chip(parent, 520, kHeaderY + 4, &s.chip_presence_lab);
  s.chip_state = make_chip(parent, 650, kHeaderY + 4, &s.chip_state_lab);

  lv_obj_t *left = lv_obj_create(parent);
  lv_obj_remove_style_all(left);
  lv_obj_set_pos(left, 16, 64);
  lv_obj_set_size(left, 380, 200);
  lv_obj_set_style_radius(left, 10, 0);
  lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(left, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(left, 1, 0);
  lv_obj_set_style_border_color(left, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lt = lv_label_create(left);
  lv_label_set_text(lt, "LAMP STATE");
  lv_obj_set_pos(lt, 16, 12);
  lv_obj_set_style_text_color(lt, lv_color_hex(ShowduinoPalette::Muted), 0);

  s.state_lab = lv_label_create(left);
  lv_label_set_text(s.state_lab, "OFF");
  lv_obj_set_pos(s.state_lab, 16, 44);
  lv_obj_set_style_text_font(s.state_lab, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s.state_lab, lv_color_hex(ShowduinoPalette::Text), 0);

  s.btn_ignite = make_btn(left, "IGNITE", 16, 100, 160, 56, PAGE_LAMP_CMD_IGNITE, false);
  s.btn_extinguish = make_btn(left, "EXTINGUISH", 192, 100, 160, 56, PAGE_LAMP_CMD_EXTINGUISH, true);
  s.feedback = lv_label_create(left);
  lv_label_set_text(s.feedback, " ");
  lv_obj_set_pos(s.feedback, 16, 168);
  lv_obj_set_width(s.feedback, 340);
  lv_obj_set_style_text_color(s.feedback, lv_color_hex(ShowduinoPalette::Muted), 0);

  lv_obj_t *right = lv_obj_create(parent);
  lv_obj_remove_style_all(right);
  lv_obj_set_pos(right, 408, 64);
  lv_obj_set_size(right, 376, 200);
  lv_obj_set_style_radius(right, 10, 0);
  lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(right, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(right, 1, 0);
  lv_obj_set_style_border_color(right, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *rt = lv_label_create(right);
  lv_label_set_text(rt, "SENSORS");
  lv_obj_set_pos(rt, 16, 12);
  lv_obj_set_style_text_color(rt, lv_color_hex(ShowduinoPalette::Muted), 0);

  static const char *sk[] = {
    "Button", "Blow", "Motion", "Mic", "Light", "Voltage"
  };
  for (int i = 0; i < 6; i++) {
    const int row = i / 2;
    const int col = i % 2;
    lv_obj_t *k = lv_label_create(right);
    lv_label_set_text(k, sk[i]);
    lv_obj_set_pos(k, 16 + col * 180, 44 + row * 44);
    lv_obj_set_style_text_color(k, lv_color_hex(ShowduinoPalette::Muted), 0);
    s.sensor_vals[i] = lv_label_create(right);
    lv_label_set_text(s.sensor_vals[i], "--");
    lv_obj_set_pos(s.sensor_vals[i], 16 + col * 180, 64 + row * 44);
    lv_obj_set_style_text_color(s.sensor_vals[i], lv_color_hex(ShowduinoPalette::Text), 0);
  }

  lv_obj_t *botL = lv_obj_create(parent);
  lv_obj_remove_style_all(botL);
  lv_obj_set_pos(botL, 16, 276);
  lv_obj_set_size(botL, 380, 140);
  lv_obj_set_style_radius(botL, 10, 0);
  lv_obj_set_style_bg_opa(botL, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(botL, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(botL, 1, 0);
  lv_obj_set_style_border_color(botL, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_clear_flag(botL, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *blt = lv_label_create(botL);
  lv_label_set_text(blt, "JEWEL / FLAME");
  lv_obj_set_pos(blt, 16, 10);
  lv_obj_set_style_text_color(blt, lv_color_hex(ShowduinoPalette::Muted), 0);
  s.btn_jewel = make_btn(botL, "JEWEL TEST", 16, 40, 110, 44, PAGE_LAMP_CMD_JEWEL_TEST, false);
  s.btn_flame = make_btn(botL, "FLAME TEST", 136, 40, 110, 44, PAGE_LAMP_CMD_FLAME_TEST, false);
  s.btn_off = make_btn(botL, "OFF", 256, 40, 100, 44, PAGE_LAMP_CMD_OFF, true);
  s.btn_refresh = make_btn(botL, "REFRESH", 16, 92, 110, 36, PAGE_LAMP_CMD_REFRESH, false);

  lv_obj_t *botR = lv_obj_create(parent);
  lv_obj_remove_style_all(botR);
  lv_obj_set_pos(botR, 408, 276);
  lv_obj_set_size(botR, 376, 140);
  lv_obj_set_style_radius(botR, 10, 0);
  lv_obj_set_style_bg_opa(botR, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(botR, lv_color_hex(ShowduinoPalette::PanelRaised), 0);
  lv_obj_set_style_border_width(botR, 1, 0);
  lv_obj_set_style_border_color(botR, lv_color_hex(ShowduinoPalette::AccentDark), 0);
  lv_obj_clear_flag(botR, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *brt = lv_label_create(botR);
  lv_label_set_text(brt, "AUDIO");
  lv_obj_set_pos(brt, 16, 10);
  lv_obj_set_style_text_color(brt, lv_color_hex(ShowduinoPalette::Muted), 0);
  s.btn_flick = make_btn(botR, "FLICK", 16, 40, 80, 44, PAGE_LAMP_CMD_AUDIO_FLICK, false);
  s.btn_ignaud = make_btn(botR, "IGNITION", 104, 40, 100, 44, PAGE_LAMP_CMD_AUDIO_IGNITE, false);
  s.btn_loop = make_btn(botR, "FLAME LOOP", 212, 40, 140, 44, PAGE_LAMP_CMD_AUDIO_LOOP, false);
  s.btn_astop = make_btn(botR, "STOP", 16, 92, 100, 36, PAGE_LAMP_CMD_AUDIO_STOP, true);

  paint();
}

void page_lamp_node_destroy(void) {
  s_active = false;
  s_cb = nullptr;
  memset(&s, 0, sizeof(s));
}

bool page_lamp_node_is_active(void) { return s_active; }

void page_lamp_node_apply_theme(void) {
  if (!s_active) return;
  paint();
}

void page_lamp_node_set_model(const DirectorLampNodeControl *model) {
  if (!model) return;
  s_model = *model;
  paint();
}
