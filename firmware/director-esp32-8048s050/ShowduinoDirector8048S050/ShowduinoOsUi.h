#ifndef SHOWDUINO_OS_UI_H
#define SHOWDUINO_OS_UI_H

#include <string.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "DirectorStatusBar.h"
#include "ShowduinoOsPalette.h"

/**
 * Showduino OS — unified LVGL 9 design system.
 * Presentation only: no protocol, runtime, or transport behaviour lives here.
 *
 * Layout contract:
 *   Status bar -> title -> summary -> primary content -> dock
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
#define OS_BTN_H             48
#define OS_CHIP_H            24
#define OS_BORDER_W          1
#define OS_HEADER_H          DirectorStatusBar::HEIGHT
#define OS_CONTENT_LEFT_W    470
#define OS_CONTENT_RIGHT_X   506
#define OS_CONTENT_RIGHT_W   282
#define OS_CONTENT_FULL_W    (SCREEN_WIDTH - (2 * OS_MARGIN))

#define OS_BODY_Y            (10 + DirectorStatusBar::HEIGHT)
#define OS_BODY_H            (OS_DOCK_Y - OS_BODY_Y - OS_GAP)
#define OS_TITLE_Y           OS_BODY_Y
#define OS_SUMMARY_Y         (OS_TITLE_Y + OS_TITLE_H + OS_GAP)
#define OS_PRIMARY_Y         (OS_SUMMARY_Y + OS_SUMMARY_H + OS_GAP)
#define OS_PRIMARY_H         (OS_DOCK_Y - OS_PRIMARY_Y - OS_GAP)

#define OS_DESK_SUMMARY_H    168
#define OS_DESK_ACTIONS_Y    (OS_BODY_Y + OS_DESK_SUMMARY_H + OS_GAP)
#define OS_DESK_ACTIONS_H    (OS_BODY_H - OS_DESK_SUMMARY_H - OS_GAP)
#define OS_ICON_SLOT_W       20

/* ---- Colour language ---------------------------------------------------- */
namespace OsColor {
  static const uint32_t Bg             = ShowduinoPalette::Background;
  static const uint32_t Panel          = ShowduinoPalette::Panel;
  static const uint32_t PanelRaised    = ShowduinoPalette::PanelRaised;
  static const uint32_t PanelBorder    = ShowduinoPalette::AccentDark;
  static const uint32_t Button         = ShowduinoPalette::PanelRaised;
  static const uint32_t ButtonBorder   = ShowduinoPalette::AccentDark;
  static const uint32_t ButtonPressed  = ShowduinoPalette::AccentDim;
  static const uint32_t Danger         = ShowduinoPalette::DangerPanel;
  static const uint32_t DangerBorder   = ShowduinoPalette::Danger;
  static const uint32_t Text           = ShowduinoPalette::Text;
  static const uint32_t TextMuted      = ShowduinoPalette::Muted;
  static const uint32_t TextDim        = ShowduinoPalette::Muted;
  static const uint32_t TextDisabled   = ShowduinoPalette::Disabled;
  static const uint32_t Title          = ShowduinoPalette::Text;
  static const uint32_t Ok             = ShowduinoPalette::Success;
  static const uint32_t Warn           = ShowduinoPalette::Warn;
  static const uint32_t Fault          = ShowduinoPalette::Danger;
  static const uint32_t Pending        = ShowduinoPalette::Pending;
  static const uint32_t Unknown        = ShowduinoPalette::AccentDark;
  static const uint32_t Accent         = ShowduinoPalette::Accent;
  static const uint32_t AccentSoft     = ShowduinoPalette::AccentBright;
  static const uint32_t ScanLine       = ShowduinoPalette::AccentDim;
  static const uint32_t DangerText     = ShowduinoPalette::DangerText;
}

struct ShowduinoOsTheme {
  lv_style_t screen;
  lv_style_t panel;
  lv_style_t panelRaised;
  lv_style_t button;
  lv_style_t buttonPressed;
  lv_style_t buttonDanger;
  lv_style_t buttonDangerPressed;
  lv_style_t buttonDisabled;
  lv_style_t title;
  lv_style_t heading;
  lv_style_t body;
  lv_style_t caption;
  lv_style_t chip;
  lv_style_t progressBg;
  lv_style_t progressIndicator;
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

    lv_style_init(&panelRaised);
    lv_style_set_bg_color(&panelRaised, lv_color_hex(OsColor::PanelRaised));
    lv_style_set_border_color(&panelRaised, lv_color_hex(OsColor::ButtonBorder));
    lv_style_set_border_width(&panelRaised, 1);
    lv_style_set_radius(&panelRaised, OS_PANEL_RADIUS);
    lv_style_set_pad_all(&panelRaised, OS_PAD);
    lv_style_set_shadow_color(&panelRaised, lv_color_hex(OsColor::Accent));
    lv_style_set_shadow_width(&panelRaised, 10);
    lv_style_set_shadow_opa(&panelRaised, LV_OPA_20);

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
    lv_style_set_bg_color(&buttonDangerPressed, lv_color_hex(ShowduinoPalette::DangerDark));
    lv_style_set_border_color(&buttonDangerPressed, lv_color_hex(OsColor::Warn));
    lv_style_set_shadow_color(&buttonDangerPressed, lv_color_hex(OsColor::DangerBorder));
    lv_style_set_shadow_width(&buttonDangerPressed, 12);
    lv_style_set_shadow_opa(&buttonDangerPressed, LV_OPA_50);
    lv_style_set_transform_width(&buttonDangerPressed, -2);
    lv_style_set_transform_height(&buttonDangerPressed, -2);

    lv_style_init(&buttonDisabled);
    lv_style_set_bg_color(&buttonDisabled, lv_color_hex(OsColor::Panel));
    lv_style_set_bg_opa(&buttonDisabled, LV_OPA_60);
    lv_style_set_border_color(&buttonDisabled, lv_color_hex(OsColor::Unknown));
    lv_style_set_border_width(&buttonDisabled, 1);
    lv_style_set_text_color(&buttonDisabled, lv_color_hex(OsColor::TextDisabled));
    lv_style_set_shadow_opa(&buttonDisabled, LV_OPA_TRANSP);

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

    lv_style_init(&chip);
    lv_style_set_bg_color(&chip, lv_color_hex(OsColor::Button));
    lv_style_set_bg_opa(&chip, LV_OPA_COVER);
    lv_style_set_border_color(&chip, lv_color_hex(OsColor::ButtonBorder));
    lv_style_set_border_width(&chip, 1);
    lv_style_set_radius(&chip, 10);
    lv_style_set_pad_hor(&chip, 8);
    lv_style_set_pad_ver(&chip, 3);
    lv_style_set_text_color(&chip, lv_color_hex(OsColor::Accent));

    lv_style_init(&progressBg);
    lv_style_set_bg_color(&progressBg, lv_color_hex(OsColor::ScanLine));
    lv_style_set_bg_opa(&progressBg, LV_OPA_COVER);
    lv_style_set_border_color(&progressBg, lv_color_hex(OsColor::PanelBorder));
    lv_style_set_border_width(&progressBg, 1);
    lv_style_set_radius(&progressBg, 4);

    lv_style_init(&progressIndicator);
    lv_style_set_bg_color(&progressIndicator, lv_color_hex(OsColor::Accent));
    lv_style_set_bg_opa(&progressIndicator, LV_OPA_COVER);
    lv_style_set_radius(&progressIndicator, 4);
    lv_style_set_shadow_color(&progressIndicator, lv_color_hex(OsColor::Accent));
    lv_style_set_shadow_width(&progressIndicator, 8);
    lv_style_set_shadow_opa(&progressIndicator, LV_OPA_40);

    ready = true;
  }

  lv_obj_t *makeScreen() {
    lv_obj_t *s = lv_obj_create(nullptr);
    lv_obj_remove_style_all(s);
    lv_obj_add_style(s, &screen, 0);
    lv_obj_set_size(s, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
  }

  lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int w, int h, bool raised = false) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_add_style(p, raised ? &panelRaised : &panel, 0);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
  }

  /** Re-enable LVGL 9 vertical finger scroll after makePanel/makeScreen clear SCROLLABLE.
   *  Scroll only engages when child extents exceed the viewport (st/sb > 0). */
  static void enableVerticalScroll(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
  }

  /** Child rows/cards inside a scroll area must not claim scroll (no overflow of their own). */
  static void disableNestedScroll(lv_obj_t *obj) {
    if (!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
  }

  lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text ? text : "");
    lv_obj_set_pos(l, x, y);
    return l;
  }

  lv_obj_t *makePageTitle(lv_obj_t *parent, const char *text, int x = 8, int y = 6) {
    lv_obj_t *accent = lv_obj_create(parent);
    lv_obj_remove_style_all(accent);
    lv_obj_set_pos(accent, x, y + 1);
    lv_obj_set_size(accent, 4, 20);
    lv_obj_set_style_bg_color(accent, lv_color_hex(OsColor::Accent), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent, 2, 0);

    lv_obj_t *l = makeLabel(parent, text, x + 12, y);
    lv_obj_add_style(l, &title, 0);
    return l;
  }

  lv_obj_t *makeHeading(lv_obj_t *parent, const char *text, int x = 8, int y = 4) {
    lv_obj_t *l = makeLabel(parent, text, x, y);
    lv_obj_add_style(l, &heading, 0);
    return l;
  }

  lv_obj_t *makeCaption(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *l = makeLabel(parent, text, x, y);
    lv_obj_add_style(l, &caption, 0);
    return l;
  }

  lv_obj_t *makeChip(lv_obj_t *parent, const char *text, int x, int y, uint32_t color = OsColor::Accent) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_add_style(c, &chip, 0);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_height(c, 24);
    lv_obj_set_width(c, LV_SIZE_CONTENT);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, text ? text : "");
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_center(l);
    return c;
  }

  lv_obj_t *makeProgress(lv_obj_t *parent, int x, int y, int w, int h, int value = 0) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_add_style(bar, &progressBg, LV_PART_MAIN);
    lv_obj_add_style(bar, &progressIndicator, LV_PART_INDICATOR);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    return bar;
  }

  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                       lv_event_cb_t cb, void *user, const char *command, bool danger = false,
                       bool scrollChain = true) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, danger ? &buttonDanger : &button, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(btn, danger ? &buttonDangerPressed : &buttonPressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(btn, &buttonDisabled, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    /* scrollChain=true: drag can scroll a parent. false: keep tap as click (E-CLEAR / E-STOP). */
    if (scrollChain) lv_obj_add_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
    else lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
    if (command) lv_obj_set_user_data(btn, (void *)command);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text ? text : "");
    lv_obj_center(lab);
    return btn;
  }

  lv_obj_t *makePageChrome(lv_obj_t *screen, const char *pageTitle, lv_obj_t **outTitleBar = nullptr) {
    lv_obj_t *titleBar = makePanel(screen, OS_MARGIN, OS_TITLE_Y, OS_CONTENT_FULL_W, OS_TITLE_H, true);
    makePageTitle(titleBar, pageTitle);
    makeChip(titleBar, "SHOWDUINO OS", OS_CONTENT_FULL_W - 150, 0);
    if (outTitleBar) *outTitleBar = titleBar;
    return makePanel(screen, OS_MARGIN, OS_SUMMARY_Y, OS_CONTENT_FULL_W, OS_SUMMARY_H);
  }

  lv_obj_t *makePrimaryPanel(lv_obj_t *screen) {
    return makePanel(screen, OS_MARGIN, OS_PRIMARY_Y, OS_CONTENT_FULL_W, OS_PRIMARY_H);
  }

  void makeDock(lv_obj_t *screen, lv_event_cb_t cb, void *user) {
    const int gap = OS_GAP;
    const int estopW = 130;
    const int navW = (SCREEN_WIDTH - 2 * OS_MARGIN - estopW - 4 * gap) / 4;
    int x = OS_MARGIN;
    makeButton(screen, "Desktop", x, OS_DOCK_Y, navW, OS_DOCK_H, cb, user, "SCREEN:DESKTOP"); x += navW + gap;
    makeButton(screen, "Live", x, OS_DOCK_Y, navW, OS_DOCK_H, cb, user, "SCREEN:LIVE"); x += navW + gap;
    makeButton(screen, "Shows", x, OS_DOCK_Y, navW, OS_DOCK_H, cb, user, "SCREEN:SHOWS"); x += navW + gap;
    makeButton(screen, "Settings", x, OS_DOCK_Y, navW, OS_DOCK_H, cb, user, "SCREEN:SETTINGS"); x += navW + gap;
    /* No scroll-chain — must never lose the tap to a parent scroll gesture. */
    makeButton(screen, "E-STOP", x, OS_DOCK_Y, estopW, OS_DOCK_H, cb, user, "EMERGENCY:STOP", true, false);
  }

  static void setTextIfChanged(lv_obj_t *lab, const char *text) {
    if (!lab || !text) return;
    const char *cur = lv_label_get_text(lab);
    if (!cur || strcmp(cur, text) != 0) lv_label_set_text(lab, text);
  }

  static void setTextColor(lv_obj_t *lab, uint32_t hex) {
    if (lab) lv_obj_set_style_text_color(lab, lv_color_hex(hex), 0);
  }

  static void setEnabled(lv_obj_t *obj, bool enabled) {
    if (!obj) return;
    if (enabled) {
      lv_obj_clear_state(obj, LV_STATE_DISABLED);
      lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    } else {
      lv_obj_add_state(obj, LV_STATE_DISABLED);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_opa(obj, LV_OPA_50, 0);
    }
  }

  static lv_obj_t *makeHairline(lv_obj_t *parent, int32_t x, int32_t y,
                               int32_t w, int32_t h, uint32_t colour,
                               lv_opa_t opacity = LV_OPA_COVER) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(colour), 0);
    lv_obj_set_style_bg_opa(line, opacity, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
  }

  /** Unlock-family corner frame. Used by system/modal pages. */
  void paintChassis(lv_obj_t *parent, uint32_t accent = OsColor::Accent,
                    uint32_t accentDark = OsColor::PanelBorder) {
    if (!parent) return;
    lv_obj_t *frame = lv_obj_create(parent);
    lv_obj_remove_style_all(frame);
    lv_obj_set_pos(frame, 8, 8);
    lv_obj_set_size(frame, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(accentDark), 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_radius(frame, 4, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    makeHairline(parent, 18, 18, 150, 2, accent, LV_OPA_70);
    makeHairline(parent, SCREEN_WIDTH - 168, 18, 150, 2, accent, LV_OPA_70);
    makeHairline(parent, 18, SCREEN_HEIGHT - 20, 150, 2, accent, LV_OPA_50);
    makeHairline(parent, SCREEN_WIDTH - 168, SCREEN_HEIGHT - 20, 150, 2, accent, LV_OPA_50);
    makeHairline(parent, 18, 18, 2, 64, accent, LV_OPA_70);
    makeHairline(parent, SCREEN_WIDTH - 20, 18, 2, 64, accent, LV_OPA_70);
    makeHairline(parent, 18, SCREEN_HEIGHT - 82, 2, 64, accent, LV_OPA_50);
    makeHairline(parent, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 82, 2, 64, accent, LV_OPA_50);
  }

  lv_obj_t *makeDialogScrim(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(OsColor::Bg), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_70, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    return root;
  }

  lv_obj_t *makeDialogBox(lv_obj_t *parent, int w, int h, bool danger = false) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, w, h);
    lv_obj_center(box);
    lv_obj_add_style(box, danger ? &panelRaised : &panel, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(danger ? OsColor::DangerBorder : OsColor::Accent), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(danger ? OsColor::Danger : OsColor::Panel), 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
  }

  lv_obj_t *makeEmptyState(lv_obj_t *parent, const char *titleText, const char *bodyText) {
    lv_obj_t *box = makePanel(parent, OS_MARGIN, OS_PRIMARY_Y, OS_CONTENT_FULL_W, 120);
    makeHeading(box, titleText ? titleText : "NOTHING TO SHOW", 10, 8);
    lv_obj_t *bodyLab = makeCaption(box, bodyText ? bodyText : "", 10, 40);
    lv_obj_set_width(bodyLab, OS_CONTENT_FULL_W - 28);
    lv_label_set_long_mode(bodyLab, LV_LABEL_LONG_WRAP);
    return box;
  }

  static void colourChip(lv_obj_t *chipObj, uint32_t colour) {
    if (!chipObj) return;
    lv_obj_set_style_border_color(chipObj, lv_color_hex(colour), 0);
    lv_obj_t *lab = lv_obj_get_child(chipObj, 0);
    if (lab) lv_obj_set_style_text_color(lab, lv_color_hex(colour), 0);
  }
};

#endif
