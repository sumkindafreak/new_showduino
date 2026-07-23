#ifndef SHOWDUINO_OS_UI_H
#define SHOWDUINO_OS_UI_H

#include <string.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "DirectorStatusBar.h"

/**
 * Showduino OS — unified design system.
 *
 * All Director pages inherit this visual language so the desktop, live view,
 * show library, device pages and settings feel like one operating system.
 * Presentation only: no transport, protocol or runtime behaviour is changed.
 *
 * Layout contract:
 *   Status Bar → Page Title → Summary → Primary → Optional Secondary → Dock
 */

/* ---- Geometry ----------------------------------------------------------- */
#define OS_MARGIN            12
#define OS_GAP               8
#define OS_PANEL_RADIUS      8
#define OS_BTN_RADIUS        8
#define OS_PAD               8
#define OS_TITLE_H           40
#define OS_SUMMARY_H         72
#define OS_DOCK_Y            402
#define OS_DOCK_H            56
#define OS_CONTENT_LEFT_W    470
#define OS_CONTENT_RIGHT_X   506
#define OS_CONTENT_RIGHT_W   282

#define OS_BODY_Y            (10 + DirectorStatusBar::HEIGHT)
#define OS_BODY_H            (OS_DOCK_Y - OS_BODY_Y - OS_GAP)
#define OS_TITLE_Y           OS_BODY_Y
#define OS_SUMMARY_Y         (OS_TITLE_Y + OS_TITLE_H + OS_GAP)
#define OS_PRIMARY_Y         (OS_SUMMARY_Y + OS_SUMMARY_H + OS_GAP)
#define OS_PRIMARY_H         (OS_DOCK_Y - OS_PRIMARY_Y - OS_GAP)

/* Desktop-specific (taller summary + actions; design reference) */
#define OS_DESK_SUMMARY_H    168
#define OS_DESK_ACTIONS_Y    (OS_BODY_Y + OS_DESK_SUMMARY_H + OS_GAP)
#define OS_DESK_ACTIONS_H    (OS_BODY_H - OS_DESK_SUMMARY_H - OS_GAP)

/* Icon slot reserved beside page titles. */
#define OS_ICON_SLOT_W       20

/* ---- Showduino OS colour language -------------------------------------- */
namespace OsColor {
  static const uint32_t Bg            = 0x020806;
  static const uint32_t Panel         = 0x07130F;
  static const uint32_t PanelRaised   = 0x0A1B15;
  static const uint32_t PanelBorder   = 0x145C43;
  static const uint32_t Button        = 0x0B241A;
  static const uint32_t ButtonBorder  = 0x1F8A63;
  static const uint32_t ButtonPressed = 0x124D38;
  static const uint32_t Danger        = 0x3A0B0B;
  static const uint32_t DangerBorder  = 0xFF4D4D;
  static const uint32_t Text          = 0xE8FFF5;
  static const uint32_t TextMuted     = 0x79A892;
  static const uint32_t TextDim       = 0xA8CDBB;
  static const uint32_t Title         = 0xEFFFF7;
  static const uint32_t Ok            = 0x39FF9A;
  static const uint32_t Warn          = 0xFFD166;
  static const uint32_t Fault         = 0xFF5A5F;
  static const uint32_t Unknown       = 0x557066;
  static const uint32_t Accent        = 0x2CFF88;
  static const uint32_t AccentSoft    = 0x18B96A;
  static const uint32_t ScanLine      = 0x0D3B2B;
}

/**
 * Shared LVGL styles for Showduino OS. One instance is owned by ShowduinoUi.
 */
struct ShowduinoOsTheme {
  lv_style_t screen;
  lv_style_t panel;
  lv_style_t button;
  lv_style_t buttonPressed;
  lv_style_t buttonDanger;
  lv_style_t buttonDangerPressed;
  lv_style_t title;
  lv_style_t heading;
  lv_style_t body;
  lv_style_t caption;
  bool ready = false;

  void begin() {
    if (ready) return;

    lv_style_init(&screen);
    lv_style_set_bg_color(&screen, lv_color_hex(OsColor::Bg));
    lv_style_set_bg_opa(&screen, LV_OPA_COVER);
    lv_style_set_text_color(&screen, lv_color_hex(OsColor::Text));

    lv_style_init(&panel);
    lv_style_set_bg_color(&panel, lv_color_hex(OsColor::Panel));
    lv_style_set_bg_opa(&panel, LV_OPA_COVER);
    lv_style_set_border_color(&panel, lv_color_hex(OsColor::PanelBorder));
    lv_style_set_border_width(&panel, 1);
    lv_style_set_radius(&panel, OS_PANEL_RADIUS);
    lv_style_set_pad_all(&panel, OS_PAD);
    lv_style_set_text_color(&panel, lv_color_hex(OsColor::Text));
    lv_style_set_shadow_color(&panel, lv_color_hex(OsColor::AccentSoft));
    lv_style_set_shadow_width(&panel, 8);
    lv_style_set_shadow_opa(&panel, LV_OPA_20);
    lv_style_set_shadow_spread(&panel, 0);

    lv_style_init(&button);
    lv_style_set_bg_color(&button, lv_color_hex(OsColor::Button));
    lv_style_set_bg_opa(&button, LV_OPA_COVER);
    lv_style_set_border_color(&button, lv_color_hex(OsColor::ButtonBorder));
    lv_style_set_border_width(&button, 1);
    lv_style_set_radius(&button, OS_BTN_RADIUS);
    lv_style_set_text_color(&button, lv_color_hex(OsColor::Title));
    lv_style_set_pad_all(&button, 10);
    lv_style_set_shadow_color(&button, lv_color_hex(OsColor::AccentSoft));
    lv_style_set_shadow_width(&button, 6);
    lv_style_set_shadow_opa(&button, LV_OPA_20);

    lv_style_init(&buttonPressed);
    lv_style_set_bg_color(&buttonPressed, lv_color_hex(OsColor::ButtonPressed));
    lv_style_set_border_color(&buttonPressed, lv_color_hex(OsColor::Accent));
    lv_style_set_border_width(&buttonPressed, 2);
    lv_style_set_shadow_color(&buttonPressed, lv_color_hex(OsColor::Accent));
    lv_style_set_shadow_width(&buttonPressed, 10);
    lv_style_set_shadow_opa(&buttonPressed, LV_OPA_40);
    lv_style_set_transform_width(&buttonPressed, -2);
    lv_style_set_transform_height(&buttonPressed, -2);

    lv_style_init(&buttonDanger);
    lv_style_set_bg_color(&buttonDanger, lv_color_hex(OsColor::Danger));
    lv_style_set_bg_opa(&buttonDanger, LV_OPA_COVER);
    lv_style_set_border_color(&buttonDanger, lv_color_hex(OsColor::DangerBorder));
    lv_style_set_border_width(&buttonDanger, 2);
    lv_style_set_radius(&buttonDanger, OS_BTN_RADIUS);
    lv_style_set_text_color(&buttonDanger, lv_color_hex(OsColor::Title));
    lv_style_set_pad_all(&buttonDanger, 10);
    lv_style_set_shadow_color(&buttonDanger, lv_color_hex(OsColor::DangerBorder));
    lv_style_set_shadow_width(&buttonDanger, 8);
    lv_style_set_shadow_opa(&buttonDanger, LV_OPA_30);

    lv_style_init(&buttonDangerPressed);
    lv_style_set_bg_color(&buttonDangerPressed, lv_color_hex(0x681313));
    lv_style_set_border_color(&buttonDangerPressed, lv_color_hex(0xFF7777));
    lv_style_set_shadow_color(&buttonDangerPressed, lv_color_hex(OsColor::DangerBorder));
    lv_style_set_shadow_width(&buttonDangerPressed, 12);
    lv_style_set_shadow_opa(&buttonDangerPressed, LV_OPA_50);
    lv_style_set_transform_width(&buttonDangerPressed, -2);
    lv_style_set_transform_height(&buttonDangerPressed, -2);

    lv_style_init(&title);
    lv_style_set_text_color(&title, lv_color_hex(OsColor::Accent));
    lv_style_set_text_letter_space(&title, 2);
    lv_style_set_text_font(&title, &lv_font_montserrat_16);

    lv_style_init(&heading);
    lv_style_set_text_color(&heading, lv_color_hex(OsColor::Title));
    lv_style_set_text_letter_space(&heading, 1);
    lv_style_set_text_font(&heading, &lv_font_montserrat_14);

    lv_style_init(&body);
    lv_style_set_text_color(&body, lv_color_hex(OsColor::Text));
    lv_style_set_text_font(&body, &lv_font_montserrat_14);

    lv_style_init(&caption);
    lv_style_set_text_color(&caption, lv_color_hex(OsColor::TextDim));
    lv_style_set_text_font(&caption, &lv_font_montserrat_14);

    ready = true;
  }

  lv_obj_t *makeScreen() {
    lv_obj_t *s = lv_obj_create(nullptr);
    lv_obj_remove_style_all(s);
    lv_obj_add_style(s, &screen, 0);
    lv_obj_set_size(s, SCREEN_WIDTH, SCREEN_HEIGHT);
    return s;
  }

  lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_add_style(p, &panel, 0);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
  }

  lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text ? text : "");
    lv_obj_set_pos(l, x, y);
    return l;
  }

  /** Large — page titles */
  lv_obj_t *makePageTitle(lv_obj_t *parent, const char *text, int x = 8, int y = 6) {
    lv_obj_t *l = makeLabel(parent, text, x + OS_ICON_SLOT_W, y);
    lv_obj_add_style(l, &title, 0);
    return l;
  }

  /** Medium — panel section headings */
  lv_obj_t *makeHeading(lv_obj_t *parent, const char *text, int x = 8, int y = 4) {
    lv_obj_t *l = makeLabel(parent, text, x, y);
    lv_obj_add_style(l, &heading, 0);
    return l;
  }

  /** Caption — field labels / secondary */
  lv_obj_t *makeCaption(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *l = makeLabel(parent, text, x, y);
    lv_obj_add_style(l, &caption, 0);
    return l;
  }

  /** Standard OS button */
  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       lv_event_cb_t cb, void *user, const char *command, bool danger = false) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, danger ? &buttonDanger : &button, 0);
    lv_obj_add_style(btn, danger ? &buttonDangerPressed : &buttonPressed, LV_STATE_PRESSED);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
    if (command) lv_obj_set_user_data(btn, (void *)command);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text ? text : "");
    lv_obj_center(lab);
    return btn;
  }

  /**
   * Standard page chrome: title strip + empty summary panel.
   * Returns summary panel; caller fills it. Primary area starts at OS_PRIMARY_Y.
   */
  lv_obj_t *makePageChrome(lv_obj_t *screen, const char *pageTitle, lv_obj_t **outTitleBar = nullptr) {
    lv_obj_t *titleBar = makePanel(screen, OS_MARGIN, OS_TITLE_Y, OS_CONTENT_LEFT_W, OS_TITLE_H);
    makePageTitle(titleBar, pageTitle);
    if (outTitleBar) *outTitleBar = titleBar;

    lv_obj_t *summary = makePanel(screen, OS_MARGIN, OS_SUMMARY_Y, OS_CONTENT_LEFT_W, OS_SUMMARY_H);
    return summary;
  }

  lv_obj_t *makePrimaryPanel(lv_obj_t *screen) {
    return makePanel(screen, OS_MARGIN, OS_PRIMARY_Y, OS_CONTENT_LEFT_W, OS_PRIMARY_H);
  }

  /**
   * Standard bottom navigation — identical on every page.
   * Desktop | Live | Shows | Settings | E-STOP
   */
  void makeDock(lv_obj_t *screen, lv_event_cb_t cb, void *user) {
    const int y = OS_DOCK_Y;
    const int gap = OS_GAP;
    const int h = OS_DOCK_H;
    const int estopW = 130;
    const int navW = (SCREEN_WIDTH - 2 * OS_MARGIN - estopW - 4 * gap) / 4;
    int x = OS_MARGIN;
    makeButton(screen, "Desktop", x, y, navW, h, cb, user, "SCREEN:DESKTOP"); x += navW + gap;
    makeButton(screen, "Live", x, y, navW, h, cb, user, "SCREEN:LIVE"); x += navW + gap;
    makeButton(screen, "Shows", x, y, navW, h, cb, user, "SCREEN:SHOWS"); x += navW + gap;
    makeButton(screen, "Settings", x, y, navW, h, cb, user, "SCREEN:SETTINGS"); x += navW + gap;
    makeButton(screen, "E-STOP", x, y, estopW, h, cb, user, "EMERGENCY:STOP", true);
  }

  static void setTextIfChanged(lv_obj_t *lab, const char *text) {
    if (!lab || !text) return;
    const char *cur = lv_label_get_text(lab);
    if (!cur || strcmp(cur, text) != 0) lv_label_set_text(lab, text);
  }

  static void setTextColor(lv_obj_t *lab, uint32_t hex) {
    if (!lab) return;
    lv_obj_set_style_text_color(lab, lv_color_hex(hex), 0);
  }
};

#endif
