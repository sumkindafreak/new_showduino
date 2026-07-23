#ifndef SHOWDUINO_OS_UI_H
#define SHOWDUINO_OS_UI_H

#include <string.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "DirectorStatusBar.h"

/**
 * Showduino OS — unified design system (Stage 7.9).
 * Desktop is the canonical visual reference. Presentation only.
 *
 * Layout contract (every page):
 *   Status Bar → Page Title → Summary → Primary → Optional Secondary → Dock
 */

/* ---- Geometry ----------------------------------------------------------- */
#define OS_MARGIN            12
#define OS_GAP               8
#define OS_PANEL_RADIUS      6
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

/* Icon slot (reserved — not drawn yet) */
#define OS_ICON_SLOT_W       20

/* ---- Colour language ---------------------------------------------------- */
namespace OsColor {
  static const uint32_t Bg         = 0x000000;
  static const uint32_t Panel      = 0x2A2A2A;
  static const uint32_t PanelBorder= 0x4B5563;
  static const uint32_t Button     = 0x3F3F46;
  static const uint32_t ButtonBorder = 0x71717A;
  static const uint32_t Danger     = 0x7F1D1D;
  static const uint32_t DangerBorder = 0xEF4444;
  static const uint32_t Text       = 0xF3F4F6;
  static const uint32_t TextMuted  = 0xA1A1AA;
  static const uint32_t TextDim    = 0xD1D5DB;
  static const uint32_t Title      = 0xFFFFFF;
  static const uint32_t Ok         = 0x4ADE80;  /* healthy */
  static const uint32_t Warn       = 0xFBBF24;  /* warning */
  static const uint32_t Fault      = 0xF87171;  /* fault */
  static const uint32_t Unknown    = 0x71717A;  /* unknown */
  static const uint32_t Accent     = 0xF87171;  /* brand red */
}

/**
 * Shared LVGL styles for Showduino OS. One instance owned by ShowduinoUi.
 */
struct ShowduinoOsTheme {
  lv_style_t screen;
  lv_style_t panel;
  lv_style_t button;
  lv_style_t buttonDanger;
  lv_style_t title;      /* Large — page titles */
  lv_style_t heading;    /* Medium — panel headings */
  lv_style_t body;       /* Normal — content */
  lv_style_t caption;    /* Small — secondary */
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

    lv_style_init(&button);
    lv_style_set_bg_color(&button, lv_color_hex(OsColor::Button));
    lv_style_set_bg_opa(&button, LV_OPA_COVER);
    lv_style_set_border_color(&button, lv_color_hex(OsColor::ButtonBorder));
    lv_style_set_border_width(&button, 1);
    lv_style_set_radius(&button, OS_BTN_RADIUS);
    lv_style_set_text_color(&button, lv_color_hex(OsColor::Title));
    lv_style_set_pad_all(&button, 10);

    lv_style_init(&buttonDanger);
    lv_style_set_bg_color(&buttonDanger, lv_color_hex(OsColor::Danger));
    lv_style_set_bg_opa(&buttonDanger, LV_OPA_COVER);
    lv_style_set_border_color(&buttonDanger, lv_color_hex(OsColor::DangerBorder));
    lv_style_set_border_width(&buttonDanger, 2);
    lv_style_set_radius(&buttonDanger, OS_BTN_RADIUS);
    lv_style_set_text_color(&buttonDanger, lv_color_hex(OsColor::Title));
    lv_style_set_pad_all(&buttonDanger, 10);

    lv_style_init(&title);
    lv_style_set_text_color(&title, lv_color_hex(OsColor::Title));
    lv_style_set_text_letter_space(&title, 1);
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
   * Future pages (Nodes, DMX, …) insert via Quick Actions / Settings until promoted.
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