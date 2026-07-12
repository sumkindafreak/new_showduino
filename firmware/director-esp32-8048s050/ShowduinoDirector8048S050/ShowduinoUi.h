#ifndef SHOWDUINO_UI_H
#define SHOWDUINO_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"

// =========================================================
// Showduino LVGL 9 UI shell
// Buttons call the callback below, so the main sketch controls transport.
// =========================================================

typedef void (*ShowduinoCommandCallback)(const String &command);

class ShowduinoUi {
public:
  void begin(ShowduinoCommandCallback callback) {
    commandCallback = callback;
    initTheme();
    buildScreens();
    showDesktop();
  }

  void setBootTime(unsigned long startedAt) { bootMs = startedAt; updateStatusWidgets(); }
  void setStageReady(bool ready) { stageReady = ready; updateStatusWidgets(); }
  void setEmergencyLocked(bool locked) { emergencyLocked = locked; updateStatusWidgets(); }
  void setTraffic(uint32_t tx, uint32_t rx) { txCount = tx; rxCount = rx; updateStatusWidgets(); }

  void appendLog(const String &line) {
    Serial.println(line);
    uiLogText += line + "\n";
    if (uiLogText.length() > 900) uiLogText = uiLogText.substring(uiLogText.length() - 900);
    if (logLabel != nullptr) lv_label_set_text(logLabel, uiLogText.c_str());
  }

  void updateStatusWidgets() {
    if (stageStatusLabel == nullptr) return;
    lv_label_set_text(stageStatusLabel, stageReady ? "STAGE: READY" : "STAGE: SEARCHING");
    lv_label_set_text(safetyStatusLabel, emergencyLocked ? "SAFETY: EMERGENCY" : "SAFETY: NORMAL");

    char uptimeText[32];
    snprintf(uptimeText, sizeof(uptimeText), "UP: %lus", (millis() - bootMs) / 1000UL);
    lv_label_set_text(uptimeLabel, uptimeText);

    char trafficText[40];
    snprintf(trafficText, sizeof(trafficText), "TX %lu / RX %lu", (unsigned long)txCount, (unsigned long)rxCount);
    lv_label_set_text(trafficLabel, trafficText);
  }

private:
  ShowduinoCommandCallback commandCallback = nullptr;
  lv_obj_t *desktopScreen = nullptr;
  lv_obj_t *liveScreen = nullptr;
  lv_obj_t *showsScreen = nullptr;
  lv_obj_t *diagnosticsScreen = nullptr;
  lv_obj_t *settingsScreen = nullptr;
  lv_obj_t *stageStatusLabel = nullptr;
  lv_obj_t *safetyStatusLabel = nullptr;
  lv_obj_t *uptimeLabel = nullptr;
  lv_obj_t *trafficLabel = nullptr;
  lv_obj_t *logLabel = nullptr;
  lv_style_t styleScreen, stylePanel, styleButton, styleDangerButton, styleProofButton, styleTitle, styleSmall;
  String uiLogText;
  bool stageReady = false;
  bool emergencyLocked = false;
  uint32_t txCount = 0;
  uint32_t rxCount = 0;
  unsigned long bootMs = 0;

  static void staticEventHandler(lv_event_t *event) {
    ShowduinoUi *ui = (ShowduinoUi *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    const char *command = (const char *)lv_obj_get_user_data(target);
    if (ui != nullptr && command != nullptr) ui->runCommand(String(command));
  }

  void runCommand(const String &command) {
    if (command == "SCREEN:DESKTOP") { showDesktop(); return; }
    if (command == "SCREEN:LIVE") { showLive(); return; }
    if (command == "SCREEN:SHOWS") { showShows(); return; }
    if (command == "SCREEN:DIAG") { showDiagnostics(); return; }
    if (command == "SCREEN:SETTINGS") { showSettings(); return; }
    if (command == "EMERGENCY:STOP") emergencyLocked = true;
    if (command == "EMERGENCY:CLEAR") emergencyLocked = false;
    updateStatusWidgets();
    if (commandCallback != nullptr) commandCallback(command);
  }

  void initTheme() {
    lv_style_init(&styleScreen);
    lv_style_set_bg_color(&styleScreen, lv_color_hex(0x050710));
    lv_style_set_bg_grad_color(&styleScreen, lv_color_hex(0x141126));
    lv_style_set_bg_grad_dir(&styleScreen, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&styleScreen, lv_color_hex(0xEAF7FF));

    lv_style_init(&stylePanel);
    lv_style_set_bg_color(&stylePanel, lv_color_hex(0x101827));
    lv_style_set_bg_opa(&stylePanel, LV_OPA_90);
    lv_style_set_border_color(&stylePanel, lv_color_hex(0x29D9FF));
    lv_style_set_border_width(&stylePanel, 2);
    lv_style_set_radius(&stylePanel, 14);
    lv_style_set_pad_all(&stylePanel, 12);

    lv_style_init(&styleButton);
    lv_style_set_bg_color(&styleButton, lv_color_hex(0x17223A));
    lv_style_set_border_color(&styleButton, lv_color_hex(0x35E6FF));
    lv_style_set_border_width(&styleButton, 2);
    lv_style_set_radius(&styleButton, 12);
    lv_style_set_text_color(&styleButton, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&styleButton, 10);

    lv_style_init(&styleDangerButton);
    lv_style_set_bg_color(&styleDangerButton, lv_color_hex(0x3B0710));
    lv_style_set_border_color(&styleDangerButton, lv_color_hex(0xFF3355));
    lv_style_set_border_width(&styleDangerButton, 2);
    lv_style_set_radius(&styleDangerButton, 12);
    lv_style_set_text_color(&styleDangerButton, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&styleDangerButton, 10);

    lv_style_init(&styleProofButton);
    lv_style_set_bg_color(&styleProofButton, lv_color_hex(0x163B24));
    lv_style_set_border_color(&styleProofButton, lv_color_hex(0x63FF8C));
    lv_style_set_border_width(&styleProofButton, 3);
    lv_style_set_radius(&styleProofButton, 14);
    lv_style_set_text_color(&styleProofButton, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&styleProofButton, 10);

    lv_style_init(&styleTitle);
    lv_style_set_text_color(&styleTitle, lv_color_hex(0xFFFFFF));
    lv_style_set_text_letter_space(&styleTitle, 2);

    lv_style_init(&styleSmall);
    lv_style_set_text_color(&styleSmall, lv_color_hex(0x9EAAC0));
  }

  lv_obj_t *makeScreen() {
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &styleScreen, 0);
    lv_obj_set_size(screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    return screen;
  }

  lv_obj_t *makePanel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_add_style(panel, &stylePanel, 0);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    return panel;
  }

  lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
  }

  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h, const char *command, bool danger = false, bool proof = false) {
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, proof ? &styleProofButton : (danger ? &styleDangerButton : &styleButton), 0);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_add_event_cb(button, staticEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(button, (void *)command);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
  }

  void createTopBar(lv_obj_t *screen, const char *title) {
    lv_obj_t *bar = makePanel(screen, 12, 10, 776, 62);
    lv_obj_t *titleLabel = makeLabel(bar, title, 10, 8);
    lv_obj_add_style(titleLabel, &styleTitle, 0);
    stageStatusLabel = makeLabel(bar, "STAGE: SEARCHING", 360, 4);
    safetyStatusLabel = makeLabel(bar, "SAFETY: NORMAL", 360, 28);
    uptimeLabel = makeLabel(bar, "UP: 0s", 600, 4);
    trafficLabel = makeLabel(bar, "TX 0 / RX 0", 600, 28);
  }

  void createDock(lv_obj_t *screen) {
    makeButton(screen, "DESKTOP", 12, 402, 120, 56, "SCREEN:DESKTOP");
    makeButton(screen, "LIVE", 142, 402, 120, 56, "SCREEN:LIVE");
    makeButton(screen, "SHOWS", 272, 402, 120, 56, "SCREEN:SHOWS");
    makeButton(screen, "DIAG", 402, 402, 120, 56, "SCREEN:DIAG");
    makeButton(screen, "SETTINGS", 532, 402, 120, 56, "SCREEN:SETTINGS");
    makeButton(screen, "E-STOP", 662, 402, 126, 56, "EMERGENCY:STOP", true);
  }

  void createLogPanel(lv_obj_t *screen) {
    lv_obj_t *panel = makePanel(screen, 506, 86, 282, 292);
    makeLabel(panel, "EVENT LOG", 8, 4);
    logLabel = lv_label_create(panel);
    lv_obj_set_pos(logLabel, 8, 34);
    lv_obj_set_size(logLabel, 250, 220);
    lv_label_set_long_mode(logLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(logLabel, "Booting Showduino OS...\n");
  }

  void buildScreens() {
    desktopScreen = makeScreen();
    createTopBar(desktopScreen, "SHOWDUINO OS 5IN");
    createDock(desktopScreen);
    createLogPanel(desktopScreen);
    lv_obj_t *main = makePanel(desktopScreen, 12, 86, 470, 292);
    makeLabel(main, "ESP32-8048S050 CONTROL SURFACE", 8, 8);
    makeLabel(main, "Tonight proof: touchscreen -> C6 -> P4 LED", 8, 38);
    makeLabel(main, "Director sends LED:TOGGLE over ESP-NOW", 8, 66);
    makeButton(main, "LIVE CONTROL", 12, 108, 190, 56, "SCREEN:LIVE");
    makeButton(main, "P4 LED TOGGLE", 222, 108, 190, 56, "LED:TOGGLE", false, true);
    makeButton(main, "DIAGNOSTICS", 12, 184, 190, 56, "SCREEN:DIAG");
    makeButton(main, "HELLO STAGE", 222, 184, 190, 56, "HELLO");

    liveScreen = makeScreen();
    createTopBar(liveScreen, "LIVE CONTROL");
    createDock(liveScreen);
    createLogPanel(liveScreen);
    lv_obj_t *live = makePanel(liveScreen, 12, 86, 470, 292);
    makeLabel(live, "FAST ACTIONS", 8, 8);
    makeButton(live, "P4 LED TOGGLE", 12, 46, 140, 52, "LED:TOGGLE", false, true);
    makeButton(live, "LED ON", 164, 46, 140, 52, "LED:ON", false, true);
    makeButton(live, "LED OFF", 316, 46, 120, 52, "LED:OFF");
    makeButton(live, "SHOW START", 12, 116, 140, 52, "SHOW:START");
    makeButton(live, "SHOW STOP", 164, 116, 140, 52, "SHOW:STOP");
    makeButton(live, "STATUS", 316, 116, 120, 52, "STATUS:REQUEST");
    makeButton(live, "HELLFIRE FX", 12, 186, 140, 52, "PIXEL:HELLFIRE");
    makeButton(live, "AUDIO 001", 164, 186, 140, 52, "AUDIO:1:PLAY:001");
    makeButton(live, "E-CLEAR", 316, 186, 120, 52, "EMERGENCY:CLEAR");

    showsScreen = makeScreen();
    createTopBar(showsScreen, "SHOW LIBRARY");
    createDock(showsScreen);
    createLogPanel(showsScreen);
    lv_obj_t *shows = makePanel(showsScreen, 12, 86, 470, 292);
    makeLabel(shows, "SHOW FILES / SD PROJECTS", 8, 8);
    makeLabel(shows, "SD browser next: /projects, /shows, /scenes, /assets", 8, 44);
    makeButton(shows, "LOAD: Chamber", 12, 100, 190, 56, "SHOW:LOAD:Chamber");
    makeButton(shows, "LOAD: Hunt", 222, 100, 190, 56, "SHOW:LOAD:Hunt");
    makeButton(shows, "DEPLOY TO STAGE", 12, 178, 210, 56, "SHOW:DEPLOY");

    diagnosticsScreen = makeScreen();
    createTopBar(diagnosticsScreen, "DIAGNOSTICS");
    createDock(diagnosticsScreen);
    createLogPanel(diagnosticsScreen);
    lv_obj_t *diag = makePanel(diagnosticsScreen, 12, 86, 470, 292);
    makeLabel(diag, "SYSTEM CHECKS", 8, 8);
    makeLabel(diag, "Proof command: LED:TOGGLE", 8, 46);
    makeLabel(diag, "Touch -> ESP-NOW -> C6 bridge -> P4 UART", 8, 78);
    makeLabel(diag, "Storage: SPI SD ready", 8, 110);
    makeButton(diag, "REQUEST STATUS", 12, 170, 190, 56, "STATUS:REQUEST");
    makeButton(diag, "P4 LED TOGGLE", 222, 170, 190, 56, "LED:TOGGLE", false, true);

    settingsScreen = makeScreen();
    createTopBar(settingsScreen, "SETTINGS");
    createDock(settingsScreen);
    createLogPanel(settingsScreen);
    lv_obj_t *settings = makePanel(settingsScreen, 12, 86, 470, 292);
    makeLabel(settings, "SHOWDUINO OS SETTINGS", 8, 8);
    makeLabel(settings, "Network: ESP-NOW portable Director", 8, 46);
    makeLabel(settings, "Fallback: service UART", 8, 78);
    makeLabel(settings, "Tonight target: P4 LED proof", 8, 110);
    makeButton(settings, "HELLO STAGE", 12, 170, 190, 56, "HELLO");
    makeButton(settings, "CLEAR E-STOP", 222, 170, 190, 56, "EMERGENCY:CLEAR");
  }

  void showDesktop() { lv_screen_load_anim(desktopScreen, LV_SCR_LOAD_ANIM_FADE_ON, 180, 0, false); updateStatusWidgets(); }
  void showLive() { lv_screen_load_anim(liveScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
  void showShows() { lv_screen_load_anim(showsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
  void showDiagnostics() { lv_screen_load_anim(diagnosticsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
  void showSettings() { lv_screen_load_anim(settingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
};

#endif
