#include "page_05_audio_node.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "showduino_theme.h"
#include "ShowduinoOsPalette.h"
#include "ShowduinoOsUi.h"
#include "DisplayTypes.h"
#include "DirectorUiText.h"

static const int16_t kHeaderY = (int16_t)OS_TITLE_Y;
static const int16_t kHeaderH = OS_TITLE_H;

struct Page05Ui {
  lv_obj_t *root;
  lv_obj_t *header;
  lv_obj_t *btn_back;
  lv_obj_t *title;
  lv_obj_t *header_accent;
  lv_obj_t *chip_presence;
  lv_obj_t *chip_presence_lab;
  lv_obj_t *chip_play;
  lv_obj_t *chip_play_lab;
  lv_obj_t *status;
  lv_obj_t *name;
  lv_obj_t *state;
  lv_obj_t *asset;
  lv_obj_t *volume;
  lv_obj_t *storage;
  lv_obj_t *codec;
  lv_obj_t *feedback;
  lv_obj_t *btn_play;
  lv_obj_t *btn_loop;
  lv_obj_t *btn_pause;
  lv_obj_t *btn_resume;
  lv_obj_t *btn_stop;
  lv_obj_t *btn_vol_dn;
  lv_obj_t *btn_vol_up;
  lv_obj_t *btn_select;
  lv_obj_t *btn_test;
  lv_obj_t *btn_details;
  lv_obj_t *mic;
  lv_obj_t *mic_title;
  lv_obj_t *mic_ready;
  lv_obj_t *mic_level;
  lv_obj_t *mic_bar_bg;
  lv_obj_t *mic_bar_fg;
  lv_obj_t *mic_floor;
  lv_obj_t *mic_trig;
  lv_obj_t *mic_last;
  lv_obj_t *btn_cal;
  lv_obj_t *btn_snd;
  lv_obj_t *btn_th_dn;
  lv_obj_t *btn_th_up;
  lv_obj_t *btn_snd_test;
  lv_obj_t *details;
  lv_obj_t *details_body;
  lv_obj_t *select;
  lv_obj_t *select_list;
  lv_obj_t *inv_btns[SHOWDUINO_AUDIO_INV_WIRE_MAX];
  lv_obj_t *btn_inv_next;
};

static Page05Ui s;
static page05_command_fn s_cb = nullptr;
static bool s_active = false;
static DirectorAudioNodeControl s_model;
static char s_asset_cmds[SHOWDUINO_AUDIO_INV_WIRE_MAX][48];

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
  lv_obj_set_size(c, 110, 28);
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
  lv_obj_center(lab);
  showduino_theme_register(btn, SHOWDUINO_THEME_ROLE_BORDER);
  return btn;
}

static void set_en(lv_obj_t *btn, bool on) {
  ShowduinoOsTheme::setEnabled(btn, on);
}

static void apply_enable() {
  const bool em = s_model.emergency;
  const bool on = s_model.online && !em;
  const bool stor = director_audio_storage_ok(&s_model);
  const bool playing = director_audio_playing(&s_model);
  const bool paused = !strcmp(s_model.state, "PAUS");
  const bool idle = !strcmp(s_model.state, "IDLE") || !strcmp(s_model.state, "FLT") ||
                    !strcmp(s_model.state, "STOP");
  const bool showLock = s_model.showRunning;
  const bool canPlay = on && stor && !showLock &&
                       (idle || paused || playing) &&
                       director_audio_has_cap(&s_model, "PLAY");
  const bool canLoop = on && stor && !showLock &&
                       director_audio_has_cap(&s_model, "LOOP") &&
                       (idle || paused || playing);
  const bool canPause = on && playing && director_audio_has_cap(&s_model, "PAUSE");
  const bool canResume = on && paused;
  const bool canStop = on && (playing || paused || !strcmp(s_model.state, "LOAD") ||
                              !strcmp(s_model.state, "STOP"));
  const bool canVol = on && director_audio_has_cap(&s_model, "VOL");
  const bool canTest = on && stor && !showLock && !em;

  set_en(s.btn_play, canPlay);
  set_en(s.btn_loop, canLoop);
  set_en(s.btn_pause, canPause);
  set_en(s.btn_resume, canResume);
  set_en(s.btn_stop, canStop);
  set_en(s.btn_vol_dn, canVol);
  set_en(s.btn_vol_up, canVol);
  set_en(s.btn_select, on);
  set_en(s.btn_test, canTest);
  set_en(s.btn_details, s_model.seen || on);
  const bool canSound = on && director_audio_has_cap(&s_model, "MIC");
  set_en(s.btn_cal, canSound);
  set_en(s.btn_snd, canSound);
  set_en(s.btn_th_dn, canSound);
  set_en(s.btn_th_up, canSound);
  set_en(s.btn_snd_test, canSound && !em);
}

static void paint() {
  if (!s_active) return;

  uint32_t presenceCol = ShowduinoPalette::Disabled;
  const char *presence = "OFFLINE";
  if (s_model.emergency) {
    presence = "EMERGENCY";
    presenceCol = ShowduinoPalette::Danger;
  } else if (!s_model.online) {
    presence = "OFFLINE";
    presenceCol = ShowduinoPalette::Disabled;
  } else if (!strcmp(s_model.state, "FLT") || !strcmp(s_model.state, "NSD")) {
    presence = "FAULT";
    presenceCol = ShowduinoPalette::Danger;
  } else {
    presence = "ONLINE";
    presenceCol = ShowduinoPalette::Success;
  }
  colour_chip(s.chip_presence, s.chip_presence_lab, presence, presenceCol);

  const char *playWord = director_audio_state_text(s_model.state);
  uint32_t playCol = ShowduinoPalette::Muted;
  if (!strcmp(playWord, "PLAYING") || !strcmp(playWord, "LOADING")) playCol = ShowduinoPalette::AccentBright;
  else if (!strcmp(playWord, "LOOPING")) playCol = ShowduinoPalette::Accent;
  else if (!strcmp(playWord, "PAUSED")) playCol = ShowduinoPalette::Warn;
  else if (!strcmp(playWord, "FAULT") || !strcmp(playWord, "NO STORAGE") ||
           !strcmp(playWord, "EMERGENCY")) {
    playCol = ShowduinoPalette::Danger;
  } else if (!strcmp(playWord, "IDLE")) {
    playCol = ShowduinoPalette::Success;
  }
  if (s_model.pending != DIRECTOR_AUDIO_PEND_NONE) {
    colour_chip(s.chip_play, s.chip_play_lab, "PENDING", ShowduinoPalette::Pending);
  } else {
    colour_chip(s.chip_play, s.chip_play_lab, playWord, playCol);
  }

  if (s.name) lv_label_set_text(s.name, "Name     Audio Node");
  char line[96];
  snprintf(line, sizeof(line), "State    %s", playWord);
  if (s.state) lv_label_set_text(s.state, line);
  char assetShown[SHOWDUINO_AUDIO_DETAIL_ASSET_MAX + 1];
  director_ui_sanitize_copy(assetShown, sizeof(assetShown),
      s_model.asset[0] ? s_model.asset
                       : (s_model.selectedAsset[0] ? s_model.selectedAsset : "-"));
  snprintf(line, sizeof(line), "Asset    %s", assetShown);
  if (s.asset) lv_label_set_text(s.asset, line);
  const uint8_t shownVol = (s_model.pending == DIRECTOR_AUDIO_PEND_VOLUME)
                               ? s_model.pendingVolume : s_model.volume;
  snprintf(line, sizeof(line), "Volume   %u%%", (unsigned)shownVol);
  if (s.volume) lv_label_set_text(s.volume, line);
  snprintf(line, sizeof(line), "Storage  %s", director_audio_storage_text(s_model.storage));
  if (s.storage) lv_label_set_text(s.storage, line);
  snprintf(line, sizeof(line), "Codec    ES8388 %s",
           !strcmp(s_model.codecHealth, "FLT") ? "FAULT" :
           (!strcmp(s_model.codecHealth, "OK") ? "READY" : "-"));
  if (s.codec) lv_label_set_text(s.codec, line);

  if (s.mic_ready) {
    const char *rdy = !s_model.online ? "OFFLINE" :
                      (!strcmp(s_model.soundReadyTok, "CAL") ? "CALIBRATING" :
                       (!strcmp(s_model.soundReadyTok, "FLT") ? "FAULT" :
                        (s_model.soundReady ? "READY" : "OFF")));
    snprintf(line, sizeof(line), "MIC / INPUT   %s", rdy);
    lv_label_set_text(s.mic_ready, line);
  }
  if (s.mic_level) {
    snprintf(line, sizeof(line), "LIVE LEVEL  %u   PEAK %u",
             (unsigned)s_model.soundLevel, (unsigned)s_model.soundPeak);
    lv_label_set_text(s.mic_level, line);
  }
  if (s.mic_bar_fg && s.mic_bar_bg) {
    int w = (int)s_model.soundLevel * 220 / 100;
    if (w < 2) w = 2;
    if (w > 220) w = 220;
    lv_obj_set_width(s.mic_bar_fg, (int16_t)w);
  }
  if (s.mic_floor) {
    snprintf(line, sizeof(line), "NOISE FLOOR  %u   TH %u",
             (unsigned)s_model.soundFloor, (unsigned)s_model.soundThreshold);
    lv_label_set_text(s.mic_floor, line);
  }
  if (s.mic_trig) {
    const char *arm = !s_model.soundEnabled ? "DISABLED" :
                      (s_model.soundCooldown ? "COOLDOWN" :
                       (s_model.soundArmed ? "ARMED" : "INHIBITED"));
    snprintf(line, sizeof(line), "TRIGGER  %s", arm);
    lv_label_set_text(s.mic_trig, line);
  }
  if (s.mic_last) {
    const char *ev = s_model.soundLastEvent[0] ? s_model.soundLastEvent : "NONE";
    if (!strcmp(ev, "TRAN")) ev = "TRANSIENT";
    else if (!strcmp(ev, "LEV")) ev = "LEVEL";
    else if (!strcmp(ev, "SUST")) ev = "SUSTAINED";
    snprintf(line, sizeof(line), "LAST EVENT  %s", ev);
    lv_label_set_text(s.mic_last, line);
  }
  if (s.btn_snd) {
    lv_obj_t *lab = lv_obj_get_child(s.btn_snd, 0);
    if (lab) lv_label_set_text(lab, s_model.soundEnabled ? "DISABLE" : "ENABLE");
  }

  const char *fb = "";
  if (s_model.emergency) fb = "EMERGENCY ACTIVE  |  attraction audio locked";
  else if (!s_model.online) fb = "Audio Node OFFLINE";
  else if (s_model.showRunning) fb = "SHOW CONTROLLED  |  PLAY/LOOP locked";
  else if (s_model.pending != DIRECTOR_AUDIO_PEND_NONE) fb = director_audio_pending_text(s_model.pending);
  else if (s_model.feedback[0]) fb = s_model.feedback;
  else if (s_model.lastErrorText[0]) fb = s_model.lastErrorText;
  if (s.feedback) {
    lv_label_set_text(s.feedback, fb);
    lv_obj_set_style_text_color(s.feedback, lv_color_hex(
        (s_model.emergency || s_model.lastErrorText[0]) ? ShowduinoPalette::Danger
        : (s_model.pending != DIRECTOR_AUDIO_PEND_NONE ? ShowduinoPalette::Pending
                                                       : ShowduinoPalette::Muted)), 0);
  }

  if (s.details_body) {
    char body[360];
    snprintf(body, sizeof(body),
             "Type        AUDIO\n"
             "Name        Audio Node\n"
             "MAC         %s\n"
             "Firmware    %s\n"
             "Protocol    1.2\n"
             "Codec       ES8388 %s\n"
             "Storage     %s\n"
             "State       %s\n"
             "Asset       %s\n"
             "Volume      %u%%\n"
             "Capabilities %s\n"
             "Fault       %s",
             s_model.mac[0] ? s_model.mac : "-",
             s_model.firmware[0] ? s_model.firmware : "-",
             !strcmp(s_model.codecHealth, "FLT") ? "FAULT" : "READY",
             director_audio_storage_text(s_model.storage),
             playWord,
             s_model.asset[0] ? s_model.asset : "-",
             (unsigned)s_model.volume,
             s_model.caps[0] ? s_model.caps : "WAV,PLAY,LOOP,STOP,VOL,PAUSE",
             s_model.lastErrorText[0] ? s_model.lastErrorText : "-");
    lv_label_set_text(s.details_body, body);
  }

  for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_WIRE_MAX; i++) {
    if (!s.inv_btns[i]) continue;
    if (s_model.inventory[i][0]) {
      lv_obj_clear_flag(s.inv_btns[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_t *lab = lv_obj_get_child(s.inv_btns[i], 0);
      if (lab) lv_label_set_text(lab, s_model.inventory[i]);
      snprintf(s_asset_cmds[i], sizeof(s_asset_cmds[i]), "PAGE05:ASSET:%s",
               s_model.inventory[i]);
      lv_obj_set_user_data(s.inv_btns[i], (void *)s_asset_cmds[i]);
    } else {
      lv_obj_add_flag(s.inv_btns[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (s.btn_inv_next) {
    ShowduinoOsTheme::setEnabled(s.btn_inv_next,
        s_model.inventoryTotal > (uint16_t)((s_model.inventoryPage + 1) * SHOWDUINO_AUDIO_INV_WIRE_MAX));
  }

  apply_enable();
}

static void back_event(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  emit(PAGE05_CMD_BACK);
}

static lv_obj_t *make_stat(lv_obj_t *parent, int16_t x, int16_t y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, 360);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(ShowduinoPalette::Text), 0);
  return l;
}

void page_05_audio_node_create(lv_obj_t *parent, page05_command_fn command_cb) {
  if (!parent) return;
  if (s_active) page_05_audio_node_destroy();
  showduino_theme_init();
  memset(&s, 0, sizeof(s));
  s_cb = command_cb;
  s.root = parent;
  if (!s_model.selectedAsset[0]) {
    director_audio_node_clear(&s_model);
  }

  s.header = lv_obj_create(parent);
  lv_obj_remove_style_all(s.header);
  lv_obj_set_pos(s.header, 0, kHeaderY);
  lv_obj_set_size(s.header, DISPLAY_WIDTH, kHeaderH);
  lv_obj_set_style_bg_opa(s.header, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s.header, LV_OBJ_FLAG_SCROLLABLE);

  s.header_accent = lv_obj_create(s.header);
  lv_obj_remove_style_all(s.header_accent);
  lv_obj_set_pos(s.header_accent, 110, 34);
  lv_obj_set_size(s.header_accent, 160, 3);
  lv_obj_set_style_bg_opa(s.header_accent, LV_OPA_COVER, 0);
  showduino_theme_register(s.header_accent, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);

  s.btn_back = lv_button_create(s.header);
  style_back(s.btn_back);
  lv_obj_set_pos(s.btn_back, 12, 4);
  lv_obj_set_size(s.btn_back, 88, 40);
  lv_obj_add_event_cb(s.btn_back, back_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *bl = lv_label_create(s.btn_back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(bl, lv_color_hex(ShowduinoPalette::Text), 0);
  lv_obj_center(bl);

  s.title = lv_label_create(s.header);
  lv_label_set_text(s.title, "AUDIO NODE");
  lv_obj_set_pos(s.title, 110, 10);
  lv_obj_set_style_text_font(s.title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(s.title, lv_color_hex(ShowduinoPalette::Text), 0);
  showduino_theme_register(s.title, SHOWDUINO_THEME_ROLE_TEXT);

  s.chip_presence = make_chip(s.header, 540, 8, &s.chip_presence_lab);
  s.chip_play = make_chip(s.header, 660, 8, &s.chip_play_lab);

  const int16_t bodyY = (int16_t)(kHeaderY + kHeaderH + OS_GAP);
  s.status = lv_obj_create(parent);
  lv_obj_remove_style_all(s.status);
  lv_obj_set_pos(s.status, 20, bodyY);
  lv_obj_set_size(s.status, 760, 108);
  ShowduinoOsTheme::styleRaisedCard(s.status, true);
  ShowduinoOsTheme::decorateCard(s.status, true);
  lv_obj_clear_flag(s.status, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(s.status, SHOWDUINO_THEME_ROLE_BORDER);

  s.name = make_stat(s.status, 16, 16);
  s.state = make_stat(s.status, 16, 40);
  s.asset = make_stat(s.status, 16, 64);
  s.volume = make_stat(s.status, 400, 16);
  s.storage = make_stat(s.status, 400, 40);
  s.codec = make_stat(s.status, 400, 64);
  s.feedback = make_stat(s.status, 16, 80);
  lv_obj_set_width(s.feedback, 720);
  lv_obj_set_style_text_color(s.feedback, lv_color_hex(ShowduinoPalette::Muted), 0);

  const int16_t micY = (int16_t)(bodyY + 116);
  s.mic = lv_obj_create(parent);
  lv_obj_remove_style_all(s.mic);
  lv_obj_set_pos(s.mic, 20, micY);
  lv_obj_set_size(s.mic, 760, 92);
  ShowduinoOsTheme::styleRaisedCard(s.mic, true);
  ShowduinoOsTheme::decorateCard(s.mic, true);
  lv_obj_clear_flag(s.mic, LV_OBJ_FLAG_SCROLLABLE);
  showduino_theme_register(s.mic, SHOWDUINO_THEME_ROLE_BORDER);
  s.mic_ready = make_stat(s.mic, 16, 8);
  s.mic_level = make_stat(s.mic, 16, 28);
  s.mic_bar_bg = lv_obj_create(s.mic);
  lv_obj_remove_style_all(s.mic_bar_bg);
  lv_obj_set_pos(s.mic_bar_bg, 400, 30);
  lv_obj_set_size(s.mic_bar_bg, 220, 12);
  lv_obj_set_style_bg_opa(s.mic_bar_bg, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s.mic_bar_bg, lv_color_hex(ShowduinoPalette::Disabled), 0);
  s.mic_bar_fg = lv_obj_create(s.mic_bar_bg);
  lv_obj_remove_style_all(s.mic_bar_fg);
  lv_obj_set_pos(s.mic_bar_fg, 0, 0);
  lv_obj_set_size(s.mic_bar_fg, 2, 12);
  lv_obj_set_style_bg_opa(s.mic_bar_fg, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s.mic_bar_fg, lv_color_hex(ShowduinoPalette::Accent), 0);
  showduino_theme_register(s.mic_bar_fg, SHOWDUINO_THEME_ROLE_HEADER_ACCENT);
  s.mic_floor = make_stat(s.mic, 16, 48);
  s.mic_trig = make_stat(s.mic, 400, 8);
  s.mic_last = make_stat(s.mic, 400, 48);

  const int16_t by = (int16_t)(micY + 100);
  s.btn_play = make_btn(parent, "PLAY", 20, by, 140, 48, PAGE05_CMD_PLAY, false);
  s.btn_loop = make_btn(parent, "LOOP", 172, by, 140, 48, PAGE05_CMD_LOOP, false);
  s.btn_pause = make_btn(parent, "PAUSE", 324, by, 140, 48, PAGE05_CMD_PAUSE, false);
  s.btn_resume = make_btn(parent, "RESUME", 476, by, 140, 48, PAGE05_CMD_RESUME, false);
  s.btn_stop = make_btn(parent, "STOP", 628, by, 132, 48, PAGE05_CMD_STOP, true);

  const int16_t by2 = (int16_t)(by + 56);
  s.btn_vol_dn = make_btn(parent, "VOL -", 20, by2, 100, 44, PAGE05_CMD_VOL_DOWN, false);
  s.btn_vol_up = make_btn(parent, "VOL +", 128, by2, 100, 44, PAGE05_CMD_VOL_UP, false);
  s.btn_select = make_btn(parent, "SELECT ASSET", 236, by2, 180, 44, PAGE05_CMD_SELECT, false);
  s.btn_test = make_btn(parent, "TEST", 424, by2, 120, 44, PAGE05_CMD_TEST, false);
  s.btn_details = make_btn(parent, "DETAILS", 552, by2, 208, 44, PAGE05_CMD_DETAILS, false);

  const int16_t by3 = (int16_t)(by2 + 48);
  s.btn_cal = make_btn(parent, "CALIBRATE", 20, by3, 140, 40, PAGE05_CMD_CALIBRATE, false);
  s.btn_snd = make_btn(parent, "ENABLE", 172, by3, 120, 40, PAGE05_CMD_SND_EN, false);
  s.btn_th_dn = make_btn(parent, "TH -", 304, by3, 80, 40, PAGE05_CMD_TH_DN, false);
  s.btn_th_up = make_btn(parent, "TH +", 396, by3, 80, 40, PAGE05_CMD_TH_UP, false);
  s.btn_snd_test = make_btn(parent, "TEST TRIGGER", 488, by3, 272, 40, PAGE05_CMD_SND_TEST, false);

  s.details = lv_obj_create(parent);
  lv_obj_remove_style_all(s.details);
  lv_obj_set_pos(s.details, 80, 70);
  lv_obj_set_size(s.details, 640, 320);
  ShowduinoOsTheme::styleRaisedCard(s.details, true);
  ShowduinoOsTheme::decorateCard(s.details, true);
  lv_obj_add_flag(s.details, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *dt = lv_label_create(s.details);
  lv_label_set_text(dt, "AUDIO NODE DETAILS");
  lv_obj_set_pos(dt, 16, 16);
  lv_obj_set_style_text_color(dt, lv_color_hex(ShowduinoPalette::Accent), 0);
  s.details_body = lv_label_create(s.details);
  lv_obj_set_pos(s.details_body, 16, 40);
  lv_obj_set_width(s.details_body, 600);
  lv_label_set_long_mode(s.details_body, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s.details_body, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s.details_body, lv_color_hex(ShowduinoPalette::Text), 0);
  make_btn(s.details, "CLOSE", 240, 268, 160, 40, PAGE05_CMD_CLOSE, false);

  s.select = lv_obj_create(parent);
  lv_obj_remove_style_all(s.select);
  lv_obj_set_pos(s.select, 140, 80);
  lv_obj_set_size(s.select, 520, 280);
  ShowduinoOsTheme::styleRaisedCard(s.select, true);
  ShowduinoOsTheme::decorateCard(s.select, true);
  lv_obj_add_flag(s.select, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *st = lv_label_create(s.select);
  lv_label_set_text(st, "SELECT ASSET");
  lv_obj_set_pos(st, 16, 16);
  lv_obj_set_style_text_color(st, lv_color_hex(ShowduinoPalette::Accent), 0);
  lv_obj_t *hint = lv_label_create(s.select);
  lv_label_set_text(hint, "Library reported by the Audio Node through the P4.");
  lv_obj_set_pos(hint, 16, 36);
  lv_obj_set_style_text_color(hint, lv_color_hex(ShowduinoPalette::Muted), 0);
  for (uint8_t i = 0; i < SHOWDUINO_AUDIO_INV_WIRE_MAX; i++) {
    s.inv_btns[i] = make_btn(s.select, "-", 20, (int16_t)(64 + i * 48), 480, 44,
                             s_asset_cmds[i], false);
    lv_obj_add_flag(s.inv_btns[i], LV_OBJ_FLAG_HIDDEN);
  }
  s.btn_inv_next = make_btn(s.select, "NEXT PAGE", 20, 214, 160, 44, PAGE05_CMD_INV_NEXT, false);
  make_btn(s.select, "CLOSE", 340, 214, 160, 44, PAGE05_CMD_CLOSE, false);

  page_05_audio_node_apply_theme();
  s_active = true;
  paint();
  Serial.println("[Page05] Audio Node control ready");
}

void page_05_audio_node_destroy(void) {
  if (!s_active && !s.root) return;
  showduino_theme_unregister(s.header_accent);
  showduino_theme_unregister(s.title);
  showduino_theme_unregister(s.btn_back);
  showduino_theme_unregister(s.status);
  if (s.root) lv_obj_clean(s.root);
  memset(&s, 0, sizeof(s));
  s_cb = nullptr;
  s_active = false;
}

bool page_05_audio_node_is_active(void) { return s_active; }

void page_05_audio_node_apply_theme(void) {
  showduino_theme_apply();
  const lv_color_t accent = showduino_theme_get_accent();
  if (s.btn_back) {
    lv_obj_set_style_border_color(s.btn_back, accent, 0);
    lv_obj_set_style_border_color(s.btn_back, accent, LV_STATE_PRESSED);
  }
  if (s.status) lv_obj_set_style_border_color(s.status, accent, 0);
}

void page_05_audio_node_set_model(const DirectorAudioNodeControl *model) {
  if (!model) return;
  s_model = *model;
  if (!s_model.selectedAsset[0]) {
    strncpy(s_model.selectedAsset, "system-test.wav", sizeof(s_model.selectedAsset) - 1);
  }
  paint();
}

void page_05_audio_node_show_details(bool show) {
  if (!s.details) return;
  if (show) lv_obj_clear_flag(s.details, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s.details, LV_OBJ_FLAG_HIDDEN);
  if (s.select) lv_obj_add_flag(s.select, LV_OBJ_FLAG_HIDDEN);
  paint();
}

void page_05_audio_node_show_select(bool show) {
  if (!s.select) return;
  if (show) lv_obj_clear_flag(s.select, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(s.select, LV_OBJ_FLAG_HIDDEN);
  if (s.details) lv_obj_add_flag(s.details, LV_OBJ_FLAG_HIDDEN);
  paint();
}

const char *page_05_audio_node_selected_asset(void) {
  return s_model.selectedAsset[0] ? s_model.selectedAsset : "system-test.wav";
}
