/*
  Showduino Director S3 - LVGL 9 Showduino OS Shell

  Purpose:
  - Turns the ESP32-S3 Director into the graphical Showduino control surface.
  - Keeps the UART link to the ESP32-P4 Stage Engine alive.
  - Keeps Serial Monitor command forwarding for bench testing.

  Required Arduino libraries:
  - lvgl 9.x
  - TFT_eSPI, with User_Setup.h configured for your display board

  Serial Monitor: 115200 baud
*/

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

// =========================================================
// Display configuration
// =========================================================
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480
#define TFT_ROTATION  1

#define HAS_BACKLIGHT 1
#define BACKLIGHT_PIN 2
#define BACKLIGHT_ON  HIGH

// Partial render buffer. Increase on boards with plenty of RAM/PSRAM.
#define LVGL_BUFFER_LINES 40

// =========================================================
// Stage Engine UART configuration
// =========================================================
#define USB_DEBUG_BAUD 115200
#define STAGE_ENGINE_BAUD 115200
#define STAGE_ENGINE_RX_PIN 19
#define STAGE_ENGINE_TX_PIN 20

#define HEARTBEAT_INTERVAL_MS   1000UL
#define HELLO_RETRY_INTERVAL_MS 5000UL
#define UI_REFRESH_INTERVAL_MS  250UL

// =========================================================
// Global objects
// =========================================================
TFT_eSPI tft = TFT_eSPI();
static lv_color_t drawBuffer[SCREEN_WIDTH * LVGL_BUFFER_LINES];
static lv_display_t *displayHandle = nullptr;

String usbInputBuffer;
String stageInputBuffer;
String uiLogText;

unsigned long lastHeartbeatMs = 0;
unsigned long lastHelloMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastLvglTickMs = 0;
unsigned long bootMs = 0;

bool stageReady = false;
bool emergencyLocked = false;
uint32_t txCount = 0;
uint32_t rxCount = 0;

// =========================================================
// LVGL screens and widgets
// =========================================================
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

lv_style_t styleScreen;
lv_style_t stylePanel;
lv_style_t styleButton;
lv_style_t styleDangerButton;
lv_style_t styleTitle;
lv_style_t styleSmall;

// =========================================================
// Forward declarations
// =========================================================
void buildAllScreens();
void showDesktop();
void showLive();
void showShows();
void showDiagnostics();
void showSettings();
void sendToStage(const String &command);
void updateStatusWidgets();
void appendLog(const String &line);

// =========================================================
// LVGL display flush callback
// =========================================================
void lvglFlush(lv_display_t *display, const lv_area_t *area, uint8_t *pixelMap) {
  uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
  uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, width, height);
  tft.pushColors((uint16_t *)pixelMap, width * height, true);
  tft.endWrite();

  lv_display_flush_ready(display);
}

// =========================================================
// Theme
// =========================================================
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

  lv_style_init(&styleTitle);
  lv_style_set_text_color(&styleTitle, lv_color_hex(0xFFFFFF));
  lv_style_set_text_letter_space(&styleTitle, 2);

  lv_style_init(&styleSmall);
  lv_style_set_text_color(&styleSmall, lv_color_hex(0x9EAAC0));
}

// =========================================================
// UI creation helpers
// =========================================================
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

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int x, int y, int w, int h, lv_event_cb_t cb, bool danger = false) {
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_add_style(button, danger ? &styleDangerButton : &styleButton, 0);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);

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
  makeButton(screen, "DESKTOP", 12, 402, 120, 56, [](lv_event_t *) { showDesktop(); });
  makeButton(screen, "LIVE", 142, 402, 120, 56, [](lv_event_t *) { showLive(); });
  makeButton(screen, "SHOWS", 272, 402, 120, 56, [](lv_event_t *) { showShows(); });
  makeButton(screen, "DIAG", 402, 402, 120, 56, [](lv_event_t *) { showDiagnostics(); });
  makeButton(screen, "SETTINGS", 532, 402, 120, 56, [](lv_event_t *) { showSettings(); });
  makeButton(screen, "E-STOP", 662, 402, 126, 56, [](lv_event_t *) { emergencyLocked = true; sendToStage("EMERGENCY:STOP"); updateStatusWidgets(); }, true);
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

// =========================================================
// Screens
// =========================================================
void buildDesktopScreen() {
  desktopScreen = makeScreen();
  createTopBar(desktopScreen, "SHOWDUINO OS");
  createDock(desktopScreen);
  createLogPanel(desktopScreen);

  lv_obj_t *main = makePanel(desktopScreen, 12, 86, 470, 292);
  makeLabel(main, "CONTROL SURFACE", 8, 8);
  makeLabel(main, "LVGL 9 desktop shell for Director S3", 8, 38);
  makeLabel(main, "Director -> P4 Stage Engine -> ESP-NOW nodes", 8, 66);

  makeButton(main, "LIVE CONTROL", 12, 108, 190, 56, [](lv_event_t *) { showLive(); });
  makeButton(main, "SHOW LIBRARY", 222, 108, 190, 56, [](lv_event_t *) { showShows(); });
  makeButton(main, "DIAGNOSTICS", 12, 184, 190, 56, [](lv_event_t *) { showDiagnostics(); });
  makeButton(main, "HELLO STAGE", 222, 184, 190, 56, [](lv_event_t *) { sendToStage("HELLO"); });
}

void buildLiveScreen() {
  liveScreen = makeScreen();
  createTopBar(liveScreen, "LIVE CONTROL");
  createDock(liveScreen);
  createLogPanel(liveScreen);

  lv_obj_t *panel = makePanel(liveScreen, 12, 86, 470, 292);
  makeLabel(panel, "FAST ACTIONS", 8, 8);
  makeButton(panel, "SHOW START", 12, 46, 140, 52, [](lv_event_t *) { sendToStage("SHOW:START"); });
  makeButton(panel, "SHOW STOP", 164, 46, 140, 52, [](lv_event_t *) { sendToStage("SHOW:STOP"); });
  makeButton(panel, "STATUS", 316, 46, 120, 52, [](lv_event_t *) { sendToStage("STATUS:REQUEST"); });

  makeButton(panel, "RELAY 1 ON", 12, 116, 140, 52, [](lv_event_t *) { sendToStage("RELAY:1:ON"); });
  makeButton(panel, "RELAY 1 OFF", 164, 116, 140, 52, [](lv_event_t *) { sendToStage("RELAY:1:OFF"); });
  makeButton(panel, "PULSE 1s", 316, 116, 120, 52, [](lv_event_t *) { sendToStage("RELAY:1:PULSE:1000"); });

  makeButton(panel, "HELLFIRE FX", 12, 186, 140, 52, [](lv_event_t *) { sendToStage("PIXEL:HELLFIRE"); });
  makeButton(panel, "AUDIO 001", 164, 186, 140, 52, [](lv_event_t *) { sendToStage("AUDIO:1:PLAY:001"); });
  makeButton(panel, "E-CLEAR", 316, 186, 120, 52, [](lv_event_t *) { emergencyLocked = false; sendToStage("EMERGENCY:CLEAR"); updateStatusWidgets(); });
}

void buildShowsScreen() {
  showsScreen = makeScreen();
  createTopBar(showsScreen, "SHOW LIBRARY");
  createDock(showsScreen);
  createLogPanel(showsScreen);

  lv_obj_t *panel = makePanel(showsScreen, 12, 86, 470, 292);
  makeLabel(panel, "SHOW FILES / SD PROJECTS", 8, 8);
  makeLabel(panel, "Next step: SD browser for /projects, /shows, /scenes and /assets", 8, 44);
  makeButton(panel, "LOAD: ZombieBurst", 12, 100, 210, 56, [](lv_event_t *) { sendToStage("SHOW:LOAD:ZombieBurst"); });
  makeButton(panel, "LOAD: Chamber", 240, 100, 180, 56, [](lv_event_t *) { sendToStage("SHOW:LOAD:Chamber"); });
  makeButton(panel, "DEPLOY TO STAGE", 12, 178, 210, 56, [](lv_event_t *) { sendToStage("SHOW:DEPLOY"); });
}

void buildDiagnosticsScreen() {
  diagnosticsScreen = makeScreen();
  createTopBar(diagnosticsScreen, "DIAGNOSTICS");
  createDock(diagnosticsScreen);
  createLogPanel(diagnosticsScreen);

  lv_obj_t *panel = makePanel(diagnosticsScreen, 12, 86, 470, 292);
  makeLabel(panel, "SYSTEM CHECKS", 8, 8);
  makeLabel(panel, "Director: ESP32-S3 LVGL interface", 8, 46);
  makeLabel(panel, "Stage: ESP32-P4 executor over UART", 8, 78);
  makeLabel(panel, "Nodes: relay / audio / FastLED via ESP-NOW bridge", 8, 110);
  makeButton(panel, "REQUEST STATUS", 12, 170, 190, 56, [](lv_event_t *) { sendToStage("STATUS:REQUEST"); });
  makeButton(panel, "SELF TEST", 222, 170, 190, 56, [](lv_event_t *) { sendToStage("SELFTEST:START"); });
}

void buildSettingsScreen() {
  settingsScreen = makeScreen();
  createTopBar(settingsScreen, "SETTINGS");
  createDock(settingsScreen);
  createLogPanel(settingsScreen);

  lv_obj_t *panel = makePanel(settingsScreen, 12, 86, 470, 292);
  makeLabel(panel, "SHOWDUINO OS SETTINGS", 8, 8);
  makeLabel(panel, "Theme: Neon / CRT / Inferno foundation", 8, 46);
  makeLabel(panel, "Network: AP + STA planned", 8, 78);
  makeLabel(panel, "Storage: SD project folders planned", 8, 110);
  makeButton(panel, "HELLO STAGE", 12, 170, 190, 56, [](lv_event_t *) { sendToStage("HELLO"); });
  makeButton(panel, "CLEAR E-STOP", 222, 170, 190, 56, [](lv_event_t *) { emergencyLocked = false; sendToStage("EMERGENCY:CLEAR"); updateStatusWidgets(); });
}

void buildAllScreens() {
  buildDesktopScreen();
  buildLiveScreen();
  buildShowsScreen();
  buildDiagnosticsScreen();
  buildSettingsScreen();
}

void showDesktop() { lv_screen_load_anim(desktopScreen, LV_SCR_LOAD_ANIM_FADE_ON, 180, 0, false); updateStatusWidgets(); }
void showLive() { lv_screen_load_anim(liveScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
void showShows() { lv_screen_load_anim(showsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
void showDiagnostics() { lv_screen_load_anim(diagnosticsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }
void showSettings() { lv_screen_load_anim(settingsScreen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false); updateStatusWidgets(); }

// =========================================================
// Status and logging
// =========================================================
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

// =========================================================
// Stage Engine serial handling
// =========================================================
void sendToStage(const String &command) {
  Serial1.println(command);
  txCount++;
  appendLog("TX -> Stage: " + command);
  updateStatusWidgets();
}

void handleStageLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  rxCount++;
  appendLog("RX <- Stage: " + line);

  if (line == "READY") stageReady = true;
  if (line == "STATUS:EMERGENCY_LOCKED") emergencyLocked = true;
  if (line == "STATUS:EMERGENCY_CLEARED") emergencyLocked = false;

  updateStatusWidgets();
}

void readStageSerial() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '\n' || c == '\r') {
      if (stageInputBuffer.length() > 0) {
        handleStageLine(stageInputBuffer);
        stageInputBuffer = "";
      }
    } else {
      stageInputBuffer += c;
      if (stageInputBuffer.length() > 220) {
        stageInputBuffer = "";
        appendLog("Stage response too long; buffer cleared.");
      }
    }
  }
}

void handleUsbLine(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command == "HELP") {
    appendLog("Commands: HELLO, STATUS:REQUEST, SHOW:START, SHOW:STOP, RELAY:1:ON, RELAY:1:OFF, EMERGENCY:STOP, EMERGENCY:CLEAR");
    return;
  }

  sendToStage(command);
}

void readUsbSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (usbInputBuffer.length() > 0) {
        handleUsbLine(usbInputBuffer);
        usbInputBuffer = "";
      }
    } else {
      usbInputBuffer += c;
      if (usbInputBuffer.length() > 180) {
        usbInputBuffer = "";
        appendLog("USB command too long; buffer cleared.");
      }
    }
  }
}

void sendHeartbeatIfDue() {
  unsigned long now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendToStage("HEARTBEAT");
  }
}

void sendHelloIfNeeded() {
  if (stageReady) return;

  unsigned long now = millis();
  if (now - lastHelloMs >= HELLO_RETRY_INTERVAL_MS) {
    lastHelloMs = now;
    sendToStage("HELLO");
  }
}

// =========================================================
// Setup and loop
// =========================================================
void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino Director S3 - LVGL Showduino OS starting...");

#if HAS_BACKLIGHT
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, BACKLIGHT_ON);
#endif

  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);

  lv_init();
  displayHandle = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(displayHandle, lvglFlush);
  lv_display_set_buffers(displayHandle, drawBuffer, nullptr, sizeof(drawBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

  initTheme();
  buildAllScreens();
  showDesktop();

  Serial1.begin(STAGE_ENGINE_BAUD, SERIAL_8N1, STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN);

  bootMs = millis();
  lastHeartbeatMs = millis();
  lastHelloMs = 0;
  lastUiRefreshMs = millis();
  lastLvglTickMs = millis();

  appendLog("Showduino OS online.");
  sendToStage("HELLO");
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastLvglTickMs;
  lastLvglTickMs = now;
  lv_tick_inc(elapsed);

  readUsbSerial();
  readStageSerial();
  sendHeartbeatIfDue();
  sendHelloIfNeeded();

  if (now - lastUiRefreshMs >= UI_REFRESH_INTERVAL_MS) {
    lastUiRefreshMs = now;
    updateStatusWidgets();
  }

  lv_timer_handler();
  delay(5);
}
