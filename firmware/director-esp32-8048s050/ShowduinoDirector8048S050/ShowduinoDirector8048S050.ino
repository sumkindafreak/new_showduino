/*
  Showduino Director 5" - ESP32-S3 RGB (JC8048W550C / 8048S050)

  Display / touch bring-up follows BankOfDadLVGL landscape pattern:
  - RGB bounce buffer
  - TAMC_GT911 → LVGL
  - DISPLAY_ROTATION 0 (landscape 800x480)

  Primary use:
  - Portable control surface
  - ESP-NOW → Communications Engine (C3) → Show Engine (P4)

  Required Arduino libraries:
  - lvgl 9.x
  - Arduino_GFX_Library
  - TAMC_GT911

  Arduino IDE:
  - Board: ESP32S3 Dev Module
  - USB CDC On Boot: Enabled
  - Flash Size: 16MB | QIO 80MHz
  - PSRAM: OPI PSRAM
  - Partition: app3M_fat9M_16MB
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Wire.h>
#include <lvgl.h>
#include <TAMC_GT911.h>
#include <esp_heap_caps.h>

#include "BoardConfig.h"
#include "lvgl_port.h"
#include "touch_lvgl.h"
#include "backlight.h"
#include "ShowduinoUi.h"
#include "EspNowTransport.h"
#include "src/StorageAPI.h"
#include "../../../protocol/showduino_state_wire.h"
#include "../../../protocol/showduino_legacy_strings.h"

// =========================================================
// RGB panel — BankOfDad bounce buffer, landscape rotation 0
// =========================================================
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  RGB_DE_PIN, RGB_VSYNC_PIN, RGB_HSYNC_PIN, RGB_PCLK_PIN,
  RGB_R0_PIN, RGB_R1_PIN, RGB_R2_PIN, RGB_R3_PIN, RGB_R4_PIN,
  RGB_G0_PIN, RGB_G1_PIN, RGB_G2_PIN, RGB_G3_PIN, RGB_G4_PIN, RGB_G5_PIN,
  RGB_B0_PIN, RGB_B1_PIN, RGB_B2_PIN, RGB_B3_PIN, RGB_B4_PIN,
  RGB_HSYNC_POLARITY, RGB_HSYNC_FRONT, RGB_HSYNC_PULSE, RGB_HSYNC_BACK,
  RGB_VSYNC_POLARITY, RGB_VSYNC_FRONT, RGB_VSYNC_PULSE, RGB_VSYNC_BACK,
  RGB_PCLK_ACTIVE_NEG, RGB_PREFER_SPEED, false,
  0, 0, RGB_BOUNCE_BUFFER
);

Arduino_RGB_Display *panel = new Arduino_RGB_Display(
  SCREEN_WIDTH, SCREEN_HEIGHT, rgbpanel, DISPLAY_ROTATION, true
);

TAMC_GT911 touchDev(TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_INT_PIN, TOUCH_RST_PIN,
                    SCREEN_WIDTH, SCREEN_HEIGHT);

ShowduinoUi ui;
ShowduinoEspNowTransport espNowTransport;

String usbInputBuffer;
String stageInputBuffer;

unsigned long lastHeartbeatMs = 0;
unsigned long lastHelloMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastLvglTickMs = 0;
unsigned long lastStageReplyMs = 0;
unsigned long lastEspNowRecoverMs = 0;
unsigned long bootMs = 0;

uint8_t linkState = LINK_SEARCHING;
bool emergencyLocked = false;
bool espNowReady = false;
bool linkLostLogged = false;
bool syncRequested = false;
uint32_t txCount = 0;
uint32_t rxCount = 0;

void markLinkDisconnected(const char *reason);
void readEspNowReplies();
void sendToStage(const String &command);
void requestStateSync();
void applyLinkState(uint8_t state) {
  uint8_t prev = linkState;
  if (linkState == state) return;
  linkState = state;
  ui.setLinkState(state);
  if (state == LINK_DISCONNECTED) {
    ui.markRelayStaleUnknown();
    syncRequested = false;
  } else if (state == LINK_READY && prev != LINK_READY) {
    requestStateSync();
  }
  ui.updateStatusWidgets(false);
}

void onStorageStatus(const StorageStatus &st) {
  static StorageBootStage lastLogged = StorageBootStage::Idle;
  if (st.stage != lastLogged) {
    lastLogged = st.stage;
    Serial.printf("[StorageUI] %s\n", st.bootMessage);
  }
}

// =========================================================
// Stage Engine command sender
// Primary route: ESP-NOW -> C3 bridge -> P4 UART.
// Backup route: direct UART for bench/service testing.
// =========================================================
bool isQuietLinkTraffic(const String &msg) {
  return msg == SHOWDUINO_LEGACY_HEARTBEAT ||
         msg == SHOWDUINO_LEGACY_ACK_HEARTBEAT ||
         msg == SHOWDUINO_LEGACY_HELLO ||
         msg == SHOWDUINO_LEGACY_READY ||
         msg == SHOWDUINO_LEGACY_SHOWDUINO_STAGE ||
         msg == "BOOT:STAGE_ENGINE_READY" ||
         msg == SHOWDUINO_WIRE_SNAPSHOT_BEGIN ||
         msg == SHOWDUINO_WIRE_SNAPSHOT_END;
}

// =========================================================
// Stage Engine response parser (UART fallback + ESP-NOW replies)
// =========================================================
void handleStageLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  rxCount++;
  lastStageReplyMs = millis();
  linkLostLogged = false;

  // Any valid Stage reply proves the link is alive (reconnects from DISCONNECTED).
  applyLinkState(LINK_READY);

  if (line == SHOWDUINO_WIRE_SNAPSHOT_BEGIN) {
    ui.beginSnapshot();
    return;
  }
  if (line == SHOWDUINO_WIRE_SNAPSHOT_END) {
    ui.endSnapshot();
    syncRequested = false;
    ui.appendLog("Sync complete");
    return;
  }

  ShowduinoShowRuntimeWire showW = showduino_parse_state_show(line.c_str());
  if (showW != SHOWDUINO_SHOW_WIRE_INVALID) {
    if (showW == SHOWDUINO_SHOW_WIRE_PLAYING) ui.setShowView(DeskShowView::Playing);
    else if (showW == SHOWDUINO_SHOW_WIRE_EMERGENCY) ui.setShowView(DeskShowView::Emergency);
    else ui.setShowView(DeskShowView::Idle);
    /* Do not treat show STATE as relay confirmation */
  }

  ShowduinoEmergencyWire emW = showduino_parse_state_emergency(line.c_str());
  if (emW != SHOWDUINO_EMERGENCY_WIRE_INVALID) {
    emergencyLocked = (emW == SHOWDUINO_EMERGENCY_WIRE_ACTIVE);
    ui.setEmergencyLocked(emergencyLocked);
  }

  /* Legacy emergency companions */
  if (line == SHOWDUINO_LEGACY_STATUS_ELOCKED) {
    emergencyLocked = true;
    ui.setEmergencyLocked(true);
  }
  if (line == SHOWDUINO_LEGACY_STATUS_ECLEARED) {
    emergencyLocked = false;
    ui.setEmergencyLocked(false);
  }

  int relayCh = 0;
  ShowduinoRelayKnowledgeWire relayK = SHOWDUINO_RELAY_WIRE_INVALID;
  if (showduino_parse_state_relay(line.c_str(), &relayCh, &relayK) == 0) {
    if (relayK == SHOWDUINO_RELAY_WIRE_ON) ui.noteConfirmedSnapshot((uint8_t)relayCh, true);
    else if (relayK == SHOWDUINO_RELAY_WIRE_OFF) ui.noteConfirmedSnapshot((uint8_t)relayCh, false);
    else if (relayK == SHOWDUINO_RELAY_WIRE_UNKNOWN) ui.applyRelayUnknown((uint8_t)relayCh);
    else if (relayK == SHOWDUINO_RELAY_WIRE_FAULT) ui.applyRelayFault((uint8_t)relayCh);
  }

  char reason[40];
  if (showduino_parse_relay_outcome(line.c_str(), SHOWDUINO_WIRE_REJECTED_RELAY_PREFIX,
                                    &relayCh, reason, sizeof(reason)) == 0) {
    ui.clearRelayPendingKeepLast((uint8_t)relayCh);
    ui.appendLog(String("Rejected R") + relayCh + ": " + reason);
  }
  if (showduino_parse_relay_outcome(line.c_str(), SHOWDUINO_WIRE_FAILED_RELAY_PREFIX,
                                    &relayCh, reason, sizeof(reason)) == 0) {
    ui.clearRelayPendingKeepLast((uint8_t)relayCh);
    ui.appendLog(String("Failed R") + relayCh + ": " + reason);
  }

  /* ACCEPTED:RELAY — request accepted, not completed; leave pending visuals */
  if (line.startsWith(SHOWDUINO_WIRE_ACCEPTED_RELAY_PREFIX)) {
    /* no confirmed mutation */
  }

  /* Do NOT treat ACK:RELAY / OK:RELAY as confirmed Stage 3 state.
     Confirmed display comes only from STATE:RELAY. */

  if (line.startsWith(SHOWDUINO_WIRE_UNSUPPORTED_PREFIX) ||
      line.startsWith(SHOWDUINO_WIRE_NOT_IMPLEMENTED_PREFIX) ||
      line.startsWith(SHOWDUINO_WIRE_NODE_UNAVAILABLE_PREFIX)) {
    /* Log only — honest failure paths */
  }

  if (!isQuietLinkTraffic(line)) {
    ui.appendLog("RX <- Stage: " + line);
  }

  ui.setEmergencyLocked(emergencyLocked);
  ui.setTraffic(txCount, rxCount);
  ui.updateStatusWidgets(false);
}

void sendToStage(const String &command) {
  bool sentByEspNow = false;

#if SHOWDUINO_USE_ESPNOW
  if (espNowReady) {
    sentByEspNow = espNowTransport.sendCommand(command);
  }
#endif

#if SHOWDUINO_USE_UART_FALLBACK
  if (!sentByEspNow) {
    Serial1.println(command);
  }
#endif

  txCount++;

  /* Emergency lock: E-STOP may set local activating feedback in UI;
     CLEAR must wait for STATE:EMERGENCY:CLEAR (do not unlock here). */
  if (command == SHOWDUINO_LEGACY_EMERGENCY_STOP) {
    emergencyLocked = true;
    ui.setEmergencyLocked(true);
  }

  ui.setTraffic(txCount, rxCount);

  if (!isQuietLinkTraffic(command)) {
    if (sentByEspNow) ui.appendLog("TX -> Stage: " + command);
    else ui.appendLog("TX -> Stage UART: " + command);
  }

  // Drain any replies that arrived during the blocking send wait.
  readEspNowReplies();
}

void requestStateSync() {
  if (syncRequested) return;
  syncRequested = true;
  ui.setSynchronising(true);
  ui.appendLog("Sync: STATUS:REQUEST");
  sendToStage(SHOWDUINO_LEGACY_STATUS_REQUEST);
}

void handleUiCommand(const String &command) {
  gStorage.setLastCommand(command.c_str());
  // Do not SD-log every tap — keeps the UI responsive. Critical actions log below.
  if (command.startsWith("EMERGENCY:") || command.startsWith("STORAGE:") ||
      command.startsWith("SHOW:") || command == "STOPALL") {
    logEvent(LogLevel::Event, LogCategory::UserAction, "UI", command.c_str());
  }

  if (command == "STORAGE:BACKUP") {
    ui.appendLog(createManualBackup() ? "Backup created" : "Backup failed");
    return;
  }
  if (command == "STORAGE:UNMOUNT") {
    ui.appendLog(safelyUnmountSD() ? "SD safely unmounted" : "Unmount failed");
    return;
  }
  if (command == "STORAGE:EXPORT") {
    ui.appendLog(exportDiagnostics() ? "Diagnostics exported" : "Diagnostics export failed");
    return;
  }
  if (command == "STORAGE:REPAIR") {
    ui.appendLog(gStorage.repairFolders() ? "Folder structure repaired" : "Repair failed");
    return;
  }
  if (command == "STORAGE:STATUS") {
    const StorageStatus &st = getStorageStatus();
    char line[160];
    snprintf(line, sizeof(line), "SD %s %s  free=%lluMB  save=%u",
             st.mounted ? "OK" : "DOWN",
             st.cardType,
             (unsigned long long)(st.freeBytes / (1024 * 1024)),
             (unsigned)st.saveState);
    ui.appendLog(line);
    return;
  }

  if (command.startsWith("SETTINGS:TIMEOUT:")) {
    static const uint8_t kPresets[] = {0, 1, 3, 5, 10, 30};
    DirectorConfig &cfg = gStorage.getConfig();
    uint8_t next = cfg.screenTimeoutMinutes;

    if (command == "SETTINGS:TIMEOUT:CYCLE") {
      uint8_t idx = 0;
      bool found = false;
      for (uint8_t i = 0; i < sizeof(kPresets); i++) {
        if (kPresets[i] == cfg.screenTimeoutMinutes) {
          idx = (uint8_t)((i + 1) % sizeof(kPresets));
          found = true;
          break;
        }
      }
      if (!found) {
        idx = 0;
        for (uint8_t i = 0; i < sizeof(kPresets); i++) {
          if (kPresets[i] > cfg.screenTimeoutMinutes) {
            idx = i;
            break;
          }
        }
      }
      next = kPresets[idx];
    } else {
      next = (uint8_t)command.substring(strlen("SETTINGS:TIMEOUT:")).toInt();
    }

    cfg.screenTimeoutMinutes = next;
    gStorage.markConfigDirty();
    gStorage.saveAllConfiguration();
    backlightConfigure(cfg.screenTimeoutMinutes, cfg.brightness);
    backlightNotifyActivity();
    ui.setScreenTimeoutMinutes(next);
    if (next == 0) ui.appendLog("Auto backlight: NEVER");
    else ui.appendLog(String("Auto backlight: ") + next + " min");
    return;
  }

  if (command == "EMERGENCY:STOP") {
    gStorage.logEmergency("UI", "EMERGENCY:STOP");
  }

  backlightNotifyActivity();
  sendToStage(command);
}

void markLinkDisconnected(const char *reason) {
  static unsigned long lastLogMs = 0;
  const bool alreadyDown = (linkState == LINK_DISCONNECTED);
  applyLinkState(LINK_DISCONNECTED);
  lastHelloMs = 0;
  lastEspNowRecoverMs = 0;

  // Rate-limit UI spam when the Stage / C3 bridge is offline.
  unsigned long now = millis();
  if (!alreadyDown || !linkLostLogged || (now - lastLogMs) > 30000UL) {
    linkLostLogged = true;
    lastLogMs = now;
    ui.appendLog(String("Stage DISCONNECTED — ") + reason);
  }
}

void readEspNowReplies() {
#if SHOWDUINO_USE_ESPNOW
  if (!espNowReady) return;
  String reply;
  while (espNowTransport.popReply(reply)) {
    handleStageLine(reply);
  }
#endif
}

void readStageSerial() {
#if SHOWDUINO_USE_UART_FALLBACK
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
        ui.appendLog("Stage response too long; buffer cleared.");
      }
    }
  }
#endif
}

// =========================================================
// USB Serial command bridge for bench testing
// =========================================================
void handleUsbLine(String command) {
  command.trim();
  if (command.length() == 0) return;

  if (command == "HELP") {
    ui.appendLog("USB commands: HELLO, STATUS:REQUEST, SHOW:START, SHOW:STOP, RELAY:1:ON, RELAY:1:OFF, EMERGENCY:STOP, EMERGENCY:CLEAR");
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
        ui.appendLog("USB command too long; buffer cleared.");
      }
    }
  }
}

// =========================================================
// Automatic Stage Engine heartbeat / hello
// =========================================================
void sendHeartbeatIfDue() {
  // Only probe with HEARTBEAT while linked. HELLO owns reconnect.
  if (linkState != LINK_READY) return;

  unsigned long now = millis();
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendToStage("HEARTBEAT");
  }
}

void sendHelloIfNeeded() {
  if (linkState == LINK_READY) return;

  unsigned long now = millis();
  if (now - lastHelloMs >= HELLO_RETRY_INTERVAL_MS) {
    lastHelloMs = now;
    sendToStage("HELLO");

    // Brief wait for READY/ACK so reconnect isn't delayed a full loop cycle.
    unsigned long t0 = millis();
    while ((millis() - t0) < 300UL && linkState != LINK_READY) {
      readEspNowReplies();
      delay(5);
    }
  }
}

void recoverEspNowIfNeeded() {
#if SHOWDUINO_USE_ESPNOW
  if (!espNowReady) return;
  if (linkState == LINK_READY) return;

  unsigned long now = millis();
  if (now - lastEspNowRecoverMs < ESPNOW_RECOVER_MS) return;
  lastEspNowRecoverMs = now;
  espNowTransport.recover();
#endif
}

void checkLinkWatchdog() {
  if (linkState != LINK_READY) return;
  if (lastStageReplyMs == 0) return;

  unsigned long now = millis();
  if (now - lastStageReplyMs < LINK_TIMEOUT_MS) return;

  markLinkDisconnected("no heartbeat reply");
}

// =========================================================
// Setup
// =========================================================
void setup() {
  Serial.begin(USB_DEBUG_BAUD);
  delay(500);

  Serial.println();
  Serial.println("Showduino Portable Director 5in - JC8048W550C starting...");

  if (!psramFound() || ESP.getPsramSize() < 700000) {
    Serial.println("FATAL: Enable OPI PSRAM + 16MB Flash + QIO 80MHz");
    while (true) delay(2000);
  }
  Serial.printf("PSRAM: %u bytes free\n", (unsigned)ESP.getFreePsram());

  backlightInit(TFT_BL_PIN);

  /* SD before RGB: heavy SPI while the panel DMA is running trips Interrupt WDT. */
  Serial.println("SHOWDUINO DIRECTOR");
  Serial.println("Storage: mounting SD before display...");
  bool storageOk = storageBegin(onStorageStatus);
  if (!storageOk) {
    Serial.println("SD CARD NOT AVAILABLE");
    Serial.println("RECOVERY MODE ACTIVE");
  } else {
    Serial.println("Storage ready");
  }

  Serial.println("Display/LVGL: BankOfDad landscape bring-up...");
  if (!lvglPortInit(panel, rgbpanel)) {
    Serial.println("LVGL port init failed — check PSRAM / bounce buffer");
    while (true) delay(1000);
  }

  /* Match BankOfDad: pass gfx logical size after panel begin. */
  touchLvglInit(touchDev, gfx->width(), gfx->height(), DISPLAY_ROTATION);
  Serial.printf("Display %ux%u rotation %d (landscape)\n",
                gfx->width(), gfx->height(), DISPLAY_ROTATION);

  /* SD ran before display — re-reset GT911 so I2C is clean. */
  touchLvglRestoreAfterSd();
  if (!touchLvglReady()) {
    Serial.println("Touch: init failed — check GT911 wiring.");
  }

  if (!storageOk && gfx) {
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(20, 20);
    gfx->println("SHOWDUINO DIRECTOR");
    gfx->setCursor(20, 50);
    gfx->println("SD RECOVERY MODE");
  }

  bootMs = millis();
  ui.setBootTime(bootMs);
  ui.begin(handleUiCommand);

  {
    const DirectorConfig &cfg = gStorage.getConfig();
    backlightConfigure(cfg.screenTimeoutMinutes, cfg.brightness);
    backlightNotifyActivity();
    ui.setScreenTimeoutMinutes(cfg.screenTimeoutMinutes);
  }

  ui.appendLog("Showduino portable Director online.");
  ui.appendLog("Panel: landscape 800x480 (BankOfDad bring-up)");
  if (gStorage.isRecoveryMode()) {
    ui.appendLog("SD CARD NOT AVAILABLE — recovery mode");
    backlightConfigure(10, 255);
    ui.setScreenTimeoutMinutes(10);
  } else {
    const StorageStatus &st = getStorageStatus();
    char line[120];
    snprintf(line, sizeof(line), "SD %s  %lluMB free", st.cardType,
             (unsigned long long)(st.freeBytes / (1024 * 1024)));
    ui.appendLog(line);
    logEvent(LogLevel::Info, LogCategory::System, "Director", "Director ready");
  }

#if SHOWDUINO_USE_ESPNOW
  espNowReady = espNowTransport.begin();
  ui.appendLog(espNowReady ? "ESP-NOW ready: targeting P4/C6 bridge." : "ESP-NOW failed: using UART fallback if wired.");
#endif

#if SHOWDUINO_USE_UART_FALLBACK
  Serial1.begin(STAGE_ENGINE_BAUD, SERIAL_8N1, STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN);
  Serial.printf("Service UART fallback: RX=%d TX=%d baud=%d\n", STAGE_ENGINE_RX_PIN, STAGE_ENGINE_TX_PIN, STAGE_ENGINE_BAUD);
#endif

  lastHeartbeatMs = millis();
  lastHelloMs = 0;
  lastUiRefreshMs = millis();
  lastLvglTickMs = millis();

  sendToStage("HELLO");
  Serial.println("Setup complete. Type HELP in Serial Monitor for bench commands.");
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastLvglTickMs;
  lastLvglTickMs = now;

  lv_tick_inc(elapsed);

  /* While blanked, poll GT911 directly so a tap can wake without LVGL. */
  if (!backlightIsOn()) {
    touchLvglPollActivity();
  }

  ui.setEmergencyLocked(emergencyLocked);
  ui.setTraffic(txCount, rxCount);
  if (now - lastUiRefreshMs >= UI_REFRESH_INTERVAL_MS) {
    lastUiRefreshMs = now;
    ui.updateStatusWidgets(true);
  } else {
    ui.updateStatusWidgets(false);
  }
  lvglPortLoop();
  backlightTick(now);

  readUsbSerial();
  readEspNowReplies();
  readStageSerial();
  recoverEspNowIfNeeded();
  sendHeartbeatIfDue();
  sendHelloIfNeeded();
  checkLinkWatchdog();
  readEspNowReplies();
  storageLoop();

  delay(2);
}
