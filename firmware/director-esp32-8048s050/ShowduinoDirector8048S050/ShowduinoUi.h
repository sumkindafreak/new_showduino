#ifndef SHOWDUINO_UI_H
#define SHOWDUINO_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include "BoardConfig.h"
#include "../../../protocol/showduino_state_wire.h"

// =========================================================
// Showduino LVGL 9 UI shell
// Buttons call the callback below, so the main sketch controls hardware/UART.
// =========================================================

typedef void (*ShowduinoCommandCallback)(const String &command);

enum class DeskRelayView : uint8_t {
  Unknown = 0,
  ConfirmedOff,
  ConfirmedOn,
  PendingOff,
  PendingOn,
  Fault
};

enum class DeskShowView : uint8_t {
  Unknown = 0,
  Idle,
  Playing,
  Emergency
};

class ShowduinoUi {
public:
  void begin(ShowduinoCommandCallback callback) {
    commandCallback = callback;
    initTheme();
    buildScreens();
    showDesktop();
  }

  void setBootTime(unsigned long startedAt) { bootMs = startedAt; }
  void setLinkState(uint8_t state) {
    if (linkState == state) return;
    linkState = state;
    statusDirty = true;
  }
  uint8_t getLinkState() const { return linkState; }
  void setEmergencyLocked(bool locked) {
    if (emergencyLocked == locked) return;
    emergencyLocked = locked;
    statusDirty = true;
  }
  void setTraffic(uint32_t tx, uint32_t rx) {
    if (txCount == tx && rxCount == rx) return;
    txCount = tx;
    rxCount = rx;
    trafficDirty = true;
  }

  void setSynchronising(bool on) {
    synchronising = on;
    statusDirty = true;
  }
  bool isSynchronising() const { return synchronising; }

  void setShowView(DeskShowView v) {
    if (showView == v) return;
    showView = v;
    statusDirty = true;
  }

  void markRelayStaleUnknown() {
    for (uint8_t i = 0; i < 8; i++) {
      relayView[i] = DeskRelayView::Unknown;
      refreshRelayButton(i);
    }
    showView = DeskShowView::Unknown;
    statusDirty = true;
  }

  void beginSnapshot() {
    snapshotActive = true;
    synchronising = true;
    statusDirty = true;
  }

  void endSnapshot() {
    snapshotActive = false;
    synchronising = false;
    statusDirty = true;
  }

  bool snapshotInProgress() const { return snapshotActive; }

  void applyConfirmedRelay(uint8_t channel, bool on) {
    if (channel < 1 || channel > 8) return;
    uint8_t idx = channel - 1;
    relayView[idx] = on ? DeskRelayView::ConfirmedOn : DeskRelayView::ConfirmedOff;
    refreshRelayButton(idx);
  }

  void applyRelayUnknown(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    relayView[channel - 1] = DeskRelayView::Unknown;
    refreshRelayButton(channel - 1);
  }

  void applyRelayFault(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    relayView[channel - 1] = DeskRelayView::Fault;
    refreshRelayButton(channel - 1);
  }

  void clearRelayPendingKeepLast(uint8_t channel) {
    if (channel < 1 || channel > 8) return;
    uint8_t idx = channel - 1;
    if (lastConfirmed[idx] == DeskRelayView::ConfirmedOn ||
        lastConfirmed[idx] == DeskRelayView::ConfirmedOff) {
      relayView[idx] = lastConfirmed[idx];
    } else {
      relayView[idx] = DeskRelayView::Unknown;
    }
    refreshRelayButton(idx);
  }

  void noteConfirmedSnapshot(uint8_t channel, bool on) {
    if (channel < 1 || channel > 8) return;
    lastConfirmed[channel - 1] = on ? DeskRelayView::ConfirmedOn : DeskRelayView::ConfirmedOff;
    applyConfirmedRelay(channel, on);
  }

  DeskRelayView getRelayView(uint8_t channel) const {
    if (channel < 1 || channel > 8) return DeskRelayView::Unknown;
    return relayView[channel - 1];
  }

  /* Legacy helpers used by older call sites — map to confirmed only */
  void setRelayState(uint8_t channel, bool on) { applyConfirmedRelay(channel, on); }

  void setAllRelaysOff() {
    for (uint8_t i = 1; i <= 8; i++) applyConfirmedRelay(i, false);
  }

  void appendLog(const String &line) {
    Serial.println(line);
    uiLogText += line + "\n";
    if (uiLogText.length() > 900) uiLogText = uiLogText.substring(uiLogText.length() - 900);
    for (uint8_t i = 0; i < logWidgetCount; i++) {
      if (logLabels[i] != nullptr) lv_label_set_text(logLabels[i], uiLogText.c_str());
    }
  }

  // Call often from loop. Only touches LVGL when something actually changed.
  void updateStatusWidgets(bool refreshTrafficAndUptime = false) {
    unsigned long now = millis();
    unsigned long uptimeSec = (now - bootMs) / 1000UL;
    bool uptimeChanged = refreshTrafficAndUptime && (uptimeSec != lastDrawnUptimeSec);
    bool drawTraffic = refreshTrafficAndUptime && trafficDirty;

    if (!statusDirty && !uptimeChanged && !drawTraffic) return;

    const char *stageText = "STAGE: SEARCHING";
    lv_color_t stageColor = lv_color_hex(0xFBBF24);  // amber
    if (synchronising) {
      stageText = "STAGE: SYNCING";
      stageColor = lv_color_hex(0x60A5FA);
    } else if (linkState == LINK_READY) {
      if (showView == DeskShowView::Playing) stageText = "STAGE: PLAYING";
      else if (showView == DeskShowView::Emergency) stageText = "STAGE: EMERGENCY";
      else if (showView == DeskShowView::Idle) stageText = "STAGE: READY";
      else stageText = "STAGE: READY";
      stageColor = lv_color_hex(0x4ADE80);
    } else if (linkState == LINK_DISCONNECTED) {
      stageText = "STAGE: DISCONNECTED";
      stageColor = lv_color_hex(0xF87171);
    }

    const char *safetyText = emergencyLocked ? "SAFETY: EMERGENCY" : "SAFETY: NORMAL";
    lv_color_t safetyColor = lv_color_hex(emergencyLocked ? 0xF87171 : 0xE5E7EB);

    char uptimeText[32];
    snprintf(uptimeText, sizeof(uptimeText), "UP: %lus", uptimeSec);

    char trafficText[40];
    snprintf(trafficText, sizeof(trafficText), "TX %lu / RX %lu", (unsigned long)txCount, (unsigned long)rxCount);

    for (uint8_t i = 0; i < statusWidgetCount; i++) {
      if (statusDirty) {
        if (stageStatusLabels[i] != nullptr) {
          const char *cur = lv_label_get_text(stageStatusLabels[i]);
          if (cur == nullptr || strcmp(cur, stageText) != 0) {
            lv_label_set_text(stageStatusLabels[i], stageText);
            lv_obj_set_style_text_color(stageStatusLabels[i], stageColor, 0);
          }
        }
        if (safetyStatusLabels[i] != nullptr) {
          const char *cur = lv_label_get_text(safetyStatusLabels[i]);
          if (cur == nullptr || strcmp(cur, safetyText) != 0) {
            lv_label_set_text(safetyStatusLabels[i], safetyText);
            lv_obj_set_style_text_color(safetyStatusLabels[i], safetyColor, 0);
          }
        }
      }
      if (uptimeChanged && uptimeLabels[i] != nullptr) {
        lv_label_set_text(uptimeLabels[i], uptimeText);
      }
      if (drawTraffic && trafficLabels[i] != nullptr) {
        lv_label_set_text(trafficLabels[i], trafficText);
      }
    }

    if (uptimeChanged) lastDrawnUptimeSec = uptimeSec;
    statusDirty = false;
    if (drawTraffic) trafficDirty = false;
  }

private:
  static const uint8_t MAX_STATUS_WIDGET_SETS = 5;

  ShowduinoCommandCallback commandCallback = nullptr;
  lv_obj_t *desktopScreen = nullptr;
  lv_obj_t *liveScreen = nullptr;
  lv_obj_t *showsScreen = nullptr;
  lv_obj_t *diagnosticsScreen = nullptr;
  lv_obj_t *settingsScreen = nullptr;
  lv_obj_t *stageStatusLabels[MAX_STATUS_WIDGET_SETS] = {};
  lv_obj_t *safetyStatusLabels[MAX_STATUS_WIDGET_SETS] = {};
  lv_obj_t *uptimeLabels[MAX_STATUS_WIDGET_SETS] = {};
  lv_obj_t *trafficLabels[MAX_STATUS_WIDGET_SETS] = {};
  lv_obj_t *logLabels[MAX_STATUS_WIDGET_SETS] = {};
  lv_obj_t *relayButtons[8] = {};
  DeskRelayView relayView[8] = {};
  DeskRelayView lastConfirmed[8] = {};
  DeskShowView showView = DeskShowView::Unknown;
  uint8_t statusWidgetCount = 0;
  uint8_t logWidgetCount = 0;
  lv_style_t styleScreen, stylePanel, styleButton, styleDangerButton, styleTitle, styleSmall;
  String uiLogText;
  uint8_t linkState = LINK_SEARCHING;
  bool emergencyLocked = false;
  bool synchronising = false;
  bool snapshotActive = false;
  bool emergencyActivating = false;
  bool statusDirty = true;
  bool trafficDirty = true;
  uint32_t txCount = 0;
  uint32_t rxCount = 0;
  unsigned long bootMs = 0;
  unsigned long lastDrawnUptimeSec = UINT32_MAX;

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

    String outbound = command;

    /* Absolute relay request from channel tap — never send TOGGLE */
    if (command.startsWith("UI:RELAY:")) {
      uint8_t ch = (uint8_t)command.substring(9).toInt();
      if (ch < 1 || ch > 8) return;
      DeskRelayView v = relayView[ch - 1];
      if (v == DeskRelayView::PendingOn || v == DeskRelayView::PendingOff) {
        appendLog(String("R") + ch + " busy (pending)");
        return;
      }
      if (v != DeskRelayView::ConfirmedOn && v != DeskRelayView::ConfirmedOff) {
        appendLog(String("R") + ch + " state unknown — wait for sync");
        return;
      }
      bool wantOn = (v == DeskRelayView::ConfirmedOff);
      lastConfirmed[ch - 1] = v;
      relayView[ch - 1] = wantOn ? DeskRelayView::PendingOn : DeskRelayView::PendingOff;
      refreshRelayButton(ch - 1);
      outbound = String("RELAY:") + ch + (wantOn ? ":ON" : ":OFF");
    } else if (command == "RELAY:ALL:OFF" || command == "STOP:ALL" || command == "SHOW:STOP") {
      for (uint8_t i = 0; i < 8; i++) {
        if (relayView[i] == DeskRelayView::ConfirmedOn ||
            relayView[i] == DeskRelayView::ConfirmedOff) {
          lastConfirmed[i] = relayView[i];
        }
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
    } else if (command == "EMERGENCY:STOP") {
      emergencyActivating = true;
      /* Immediate operator feedback — authoritative lock follows STATE:EMERGENCY */
      emergencyLocked = true;
      for (uint8_t i = 0; i < 8; i++) {
        relayView[i] = DeskRelayView::PendingOff;
        refreshRelayButton(i);
      }
    } else if (command == "EMERGENCY:CLEAR") {
      /* Do not clear lock until STATE:EMERGENCY:CLEAR */
      appendLog("E-CLEAR requested…");
    }

    statusDirty = true;
    updateStatusWidgets(true);
    if (commandCallback != nullptr) commandCallback(outbound);
  }

  void refreshRelayButton(uint8_t idx) {
    if (idx >= 8 || relayButtons[idx] == nullptr) return;
    DeskRelayView v = relayView[idx];
    uint32_t bg = 0x3F3F46;
    uint32_t border = 0x71717A;
    switch (v) {
      case DeskRelayView::ConfirmedOn:
        bg = 0xB91C1C; border = 0xEF4444; break;
      case DeskRelayView::ConfirmedOff:
        bg = 0x3F3F46; border = 0x71717A; break;
      case DeskRelayView::PendingOn:
      case DeskRelayView::PendingOff:
        bg = 0x1E3A5F; border = 0x60A5FA; break;
      case DeskRelayView::Fault:
        bg = 0x7C2D12; border = 0xF59E0B; break;
      case DeskRelayView::Unknown:
      default:
        bg = 0x27272A; border = 0x52525B; break;
    }
    lv_obj_set_style_bg_color(relayButtons[idx], lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(relayButtons[idx], lv_color_hex(border), 0);
  }

  void initTheme() {
    // Black background, grey panels, high-contrast text (readable on 5" RGB)
    lv_style_init(&styleScreen);
    lv_style_set_bg_color(&styleScreen, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&styleScreen, LV_OPA_COVER);
    lv_style_set_text_color(&styleScreen, lv_color_hex(0xF3F4F6));

    lv_style_init(&stylePanel);
    lv_style_set_bg_color(&stylePanel, lv_color_hex(0x2A2A2A));
    lv_style_set_bg_opa(&stylePanel, LV_OPA_COVER);
    lv_style_set_border_color(&stylePanel, lv_color_hex(0x4B5563));
    lv_style_set_border_width(&stylePanel, 1);
    lv_style_set_radius(&stylePanel, 8);
    lv_style_set_pad_all(&stylePanel, 12);
    lv_style_set_text_color(&stylePanel, lv_color_hex(0xF3F4F6));

    lv_style_init(&styleButton);
    lv_style_set_bg_color(&styleButton, lv_color_hex(0x3F3F46));
    lv_style_set_bg_opa(&styleButton, LV_OPA_COVER);
    lv_style_set_border_color(&styleButton, lv_color_hex(0x71717A));
    lv_style_set_border_width(&styleButton, 1);
    lv_style_set_radius(&styleButton, 8);
    lv_style_set_text_color(&styleButton, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&styleButton, 10);

    lv_style_init(&styleDangerButton);
    lv_style_set_bg_color(&styleDangerButton, lv_color_hex(0x7F1D1D));
    lv_style_set_bg_opa(&styleDangerButton, LV_OPA_COVER);
    lv_style_set_border_color(&styleDangerButton, lv_color_hex(0xEF4444));
    lv_style_set_border_width(&styleDangerButton, 2);
    lv_style_set_radius(&styleDangerButton, 8);
    lv_style_set_text_color(&styleDangerButton, lv_color_hex(0xFFFFFF));
    lv_style_set_pad_all(&styleDangerButton, 10);

    lv_style_init(&styleTitle);
    lv_style_set_text_color(&styleTitle, lv_color_hex(0xFFFFFF));
    lv_style_set_text_letter_space(&styleTitle, 1);

    lv_style_init(&styleSmall);
    lv_style_set_text_color(&styleSmall, lv_color_hex(0xD1D5DB));
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

  lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h, const char *command, bool danger = false) {
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, danger ? &styleDangerButton : &styleButton, 0);
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

    lv_obj_t *stageLabel = makeLabel(bar, "STAGE: SEARCHING", 340, 4);
    lv_obj_set_style_text_color(stageLabel, lv_color_hex(0xFBBF24), 0);

    lv_obj_t *safetyLabel = makeLabel(bar, "SAFETY: NORMAL", 340, 28);
    lv_obj_set_style_text_color(safetyLabel, lv_color_hex(0xE5E7EB), 0);

    lv_obj_t *uptimeLabel = makeLabel(bar, "UP: 0s", 580, 4);
    lv_obj_add_style(uptimeLabel, &styleSmall, 0);

    lv_obj_t *trafficLabel = makeLabel(bar, "TX 0 / RX 0", 580, 28);
    lv_obj_add_style(trafficLabel, &styleSmall, 0);

    if (statusWidgetCount < MAX_STATUS_WIDGET_SETS) {
      stageStatusLabels[statusWidgetCount] = stageLabel;
      safetyStatusLabels[statusWidgetCount] = safetyLabel;
      uptimeLabels[statusWidgetCount] = uptimeLabel;
      trafficLabels[statusWidgetCount] = trafficLabel;
      statusWidgetCount++;
    }
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
    lv_obj_t *logTitle = makeLabel(panel, "EVENT LOG", 8, 4);
    lv_obj_add_style(logTitle, &styleTitle, 0);
    lv_obj_t *logLabel = lv_label_create(panel);
    lv_obj_set_pos(logLabel, 8, 34);
    lv_obj_set_size(logLabel, 250, 220);
    lv_obj_set_style_text_color(logLabel, lv_color_hex(0xE5E7EB), 0);
    lv_label_set_long_mode(logLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text(logLabel, "Booting Showduino OS...\n");
    if (logWidgetCount < MAX_STATUS_WIDGET_SETS) {
      logLabels[logWidgetCount++] = logLabel;
    }
  }

  void buildScreens() {
    desktopScreen = makeScreen();
    createTopBar(desktopScreen, "SHOWDUINO OS 5IN");
    createDock(desktopScreen);
    createLogPanel(desktopScreen);
    lv_obj_t *main = makePanel(desktopScreen, 12, 86, 470, 292);
    makeLabel(main, "CONTROL SURFACE", 8, 8);
    makeButton(main, "LIVE CONTROL", 12, 56, 190, 56, "SCREEN:LIVE");
    makeButton(main, "SHOW LIBRARY", 222, 56, 190, 56, "SCREEN:SHOWS");
    makeButton(main, "DIAGNOSTICS", 12, 132, 190, 56, "SCREEN:DIAG");
    makeButton(main, "HELLO STAGE", 222, 132, 190, 56, "HELLO");

    liveScreen = makeScreen();
    createTopBar(liveScreen, "LIVE CONTROL");
    createDock(liveScreen);
    createLogPanel(liveScreen);
    lv_obj_t *live = makePanel(liveScreen, 12, 86, 470, 292);
    makeLabel(live, "OPERATOR DESK", 8, 4);
    makeButton(live, "SHOW START", 12, 36, 108, 44, "SHOW:START");
    makeButton(live, "SHOW STOP", 128, 36, 108, 44, "SHOW:STOP");
    makeButton(live, "STATUS", 244, 36, 100, 44, "STATUS:REQUEST");
    makeButton(live, "STOP ALL", 352, 36, 100, 44, "STOP:ALL", true);

    makeLabel(live, "RELAY NODE — tap for ON/OFF (confirmed)", 8, 90);
    const char *relayCmds[8] = {
      "UI:RELAY:1", "UI:RELAY:2", "UI:RELAY:3", "UI:RELAY:4",
      "UI:RELAY:5", "UI:RELAY:6", "UI:RELAY:7", "UI:RELAY:8"
    };
    const char *relayNames[8] = { "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8" };
    for (uint8_t i = 0; i < 8; i++) {
      int row = i / 4;
      int col = i % 4;
      int x = 12 + col * 112;
      int y = 118 + row * 56;
      relayButtons[i] = makeButton(live, relayNames[i], x, y, 104, 48, relayCmds[i]);
      refreshRelayButton(i);
    }

    makeButton(live, "ALL OFF", 12, 236, 100, 40, "RELAY:ALL:OFF");
    makeButton(live, "PULSE R1", 120, 236, 100, 40, "RELAY:1:PULSE:1000");
    makeButton(live, "E-CLEAR", 228, 236, 100, 40, "EMERGENCY:CLEAR");
    makeButton(live, "HELLFIRE", 336, 236, 116, 40, "PIXEL:HELLFIRE");

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
    makeLabel(diag, "SYSTEM / STORAGE", 8, 4);
    makeLabel(diag, "Board: ESP32-8048S043 / 8048S050", 8, 36);
    makeLabel(diag, "Display: ST7262 RGB 800x480", 8, 60);
    makeLabel(diag, "Path: Director -> C3 -> Stage -> Nodes", 8, 84);
    makeButton(diag, "SD STATUS", 12, 120, 140, 44, "STORAGE:STATUS");
    makeButton(diag, "BACKUP", 160, 120, 140, 44, "STORAGE:BACKUP");
    makeButton(diag, "EXPORT DIAG", 308, 120, 140, 44, "STORAGE:EXPORT");
    makeButton(diag, "REPAIR DIRS", 12, 172, 140, 44, "STORAGE:REPAIR");
    makeButton(diag, "STAGE STATUS", 160, 172, 140, 44, "STATUS:REQUEST");
    makeButton(diag, "SELF TEST", 308, 172, 140, 44, "SELFTEST:START");

    settingsScreen = makeScreen();
    createTopBar(settingsScreen, "SETTINGS");
    createDock(settingsScreen);
    createLogPanel(settingsScreen);
    lv_obj_t *settings = makePanel(settingsScreen, 12, 86, 470, 292);
    makeLabel(settings, "SHOWDUINO OS SETTINGS", 8, 8);
    makeLabel(settings, "Storage: /showduino on SD", 8, 46);
    makeLabel(settings, "Autosave + atomic JSON writes enabled", 8, 78);
    makeLabel(settings, "NVS holds boot recovery markers only", 8, 110);
    makeButton(settings, "HELLO STAGE", 12, 170, 190, 56, "HELLO");
    makeButton(settings, "CLEAR E-STOP", 222, 170, 190, 56, "EMERGENCY:CLEAR");
    makeButton(settings, "CREATE BACKUP", 12, 236, 140, 48, "STORAGE:BACKUP");
    makeButton(settings, "EXPORT DIAG", 160, 236, 140, 48, "STORAGE:EXPORT");
    makeButton(settings, "UNMOUNT SD", 308, 236, 140, 48, "STORAGE:UNMOUNT");
  }

  void showDesktop() {
    lv_screen_load(desktopScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showLive() {
    lv_screen_load(liveScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showShows() {
    lv_screen_load(showsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showDiagnostics() {
    lv_screen_load(diagnosticsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
  void showSettings() {
    lv_screen_load(settingsScreen);
    statusDirty = true;
    trafficDirty = true;
    updateStatusWidgets(true);
  }
};

#endif
