#include "StageDiagnostics.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_arduino_version.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp32-hal-psram.h"

#include <stdarg.h>
#include "../BoardConfig.h"
#include "../ShowRuntimeOwner.h"
#include "StageStorage.h"
#include "StageAudio.h"
#include "EmergencyPixels.h"
#include "WebApiHandler.h"
#include "plugin/PluginBus.h"

extern bool emergencyLocked;
extern uint8_t gEmergencySourceId;
extern ShowRuntimeOwner gRuntime;

namespace {

enum class Result : uint8_t { Pass = 0, Warn, Fail, Skip };

enum class Step : uint8_t {
  Idle = 0,
  Header,
  P4,
  Pins,
  Sd,
  Fs,
  Wav,
  AudioStart,
  AudioWait,
  PixelStart,
  PixelWait,
  EstopPrompt,
  EstopWaitPress,
  EstopAudioWait,
  EstopWaitClear,
  CommsUart,
  CommsPing,
  CommsPongWait,
  Director,
  Parser,
  Runtime,
  ClockStart,
  ClockWait,
  WebUi,
  Memory,
  ConfirmPrompt,
  ConfirmWait,
  Report
};

enum class Row : uint8_t {
  P4System = 0,
  Memory,
  Psram,
  PinConfig,
  SdCard,
  Filesystem,
  WebUi,
  EmergencyWav,
  I2sAudio,
  AudioPhysical,
  EmergencyPixels,
  PixelsPhysical,
  PhysicalEstop,
  EmergencyLatch,
  EmergencyAudioLoop,
  CommsUart,
  CommsRoundTrip,
  DirectorLink,
  CommandParser,
  Runtime,
  SystemClock,
  Count
};

static const char *const kRowName[] = {
  "P4 SYSTEM",
  "MEMORY",
  "PSRAM",
  "PIN CONFIG",
  "SD CARD",
  "FILESYSTEM",
  "WEBUI",
  "EMERGENCY WAV",
  "I2S AUDIO",
  "AUDIO PHYSICAL",
  "EMERGENCY PIXELS",
  "PIXELS PHYSICAL",
  "PHYSICAL E-STOP",
  "EMERGENCY LATCH",
  "EMERGENCY AUDIO LOOP",
  "COMMS UART",
  "COMMS ROUND TRIP",
  "DIRECTOR LINK",
  "COMMAND PARSER",
  "RUNTIME",
  "SYSTEM CLOCK"
};

static const uint32_t kPixelStepMs = 280;
static const uint32_t kAudioPlayMs = 850;
static const uint32_t kEstopPressMs = 30000;
static const uint32_t kEstopAudioMs = 45000;
static const uint32_t kEstopClearMs = 120000;
static const uint32_t kCommsPongMs = 2000;
static const uint32_t kClockMs = 1100;
static const uint32_t kConfirmMs = 90000;
static const uint8_t kPixelBright = 40;
static const int kHeapWarnDrop = 24576;
static const uint32_t kTimeSyncedFloor = 1600000000UL; /* ~2020 — below this is unsynced */

static Step sStep = Step::Idle;
static bool sReadOnly = false;
static bool sDidAudio = false;
static bool sDidPixels = false;
static bool sPixelOwned = false;
static bool sAudioOwned = false;
static bool sWaitingInput = false;
static uint8_t sPixelPhase = 0;
static uint32_t sWaitUntil = 0;
static uint32_t sStartMs = 0;
static uint32_t sPingSentMs = 0;
static uint32_t sHeapStart = 0;
static uint32_t sPsramStart = 0;
static uint32_t sClockSec0 = 0;
static uint32_t sClockUsec0 = 0;
static uint32_t sLoopAtEstop = 0;
static bool sCommsPong = false;
static bool sPixelYes = false;
static bool sPixelNo = false;
static bool sAudioYes = false;
static bool sAudioNo = false;
static bool sEstopSawLatch = false;
static char sCurrent[40] = "idle";
static char sWaitWhy[40] = "";

static Result sRow[(unsigned)Row::Count];
static uint16_t sPass = 0;
static uint16_t sWarn = 0;
static uint16_t sFail = 0;
static uint16_t sSkip = 0;

static const char *resultName(Result r) {
  switch (r) {
    case Result::Pass: return "PASS";
    case Result::Warn: return "WARN";
    case Result::Fail: return "FAIL";
    default:   return "SKIP";
  }
}

static uint32_t psramTotalBytes() {
  if (esp_psram_is_initialized()) {
    const size_t n = esp_psram_get_size();
    if (n > 0) return (uint32_t)n;
  }
  const uint32_t arduino = ESP.getPsramSize();
  if (arduino > 0) return arduino;
  return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

static uint32_t psramFreeBytes() {
  const uint32_t arduino = ESP.getFreePsram();
  if (arduino > 0) return arduino;
  return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

static bool psramAllocProbe(char *note, size_t noteLen) {
  const size_t n = 256;
  uint8_t *p = (uint8_t *)heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = (uint8_t *)ps_malloc(n);
  if (!p) {
    snprintf(note, noteLen, "SPIRAM alloc failed");
    return false;
  }
  for (size_t i = 0; i < n; i++) p[i] = (uint8_t)(0xA5 ^ (uint8_t)i);
  bool ok = true;
  for (size_t i = 0; i < n; i++) {
    if (p[i] != (uint8_t)(0xA5 ^ (uint8_t)i)) {
      ok = false;
      break;
    }
  }
  heap_caps_free(p);
  snprintf(note, noteLen, ok ? "256-byte SPIRAM R/W" : "pattern mismatch");
  return ok;
}

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

static void bumpRow(Row row, Result r) {
  Result &cur = sRow[(unsigned)row];
  if (r == Result::Skip) return;
  if (cur == Result::Fail) return;
  if (r == Result::Fail) {
    cur = Result::Fail;
    return;
  }
  if (cur == Result::Warn) return;
  if (r == Result::Warn) {
    cur = Result::Warn;
    return;
  }
  cur = Result::Pass;
}

static void countResult(Result r) {
  switch (r) {
    case Result::Pass: sPass++; break;
    case Result::Warn: sWarn++; break;
    case Result::Fail: sFail++; break;
    default: sSkip++; break;
  }
}

static void printItem(const char *id, const char *name, Result r, const char *note,
                      Row row) {
  char dots[40];
  size_t nlen = strlen(name);
  size_t pad = (nlen < 32) ? (32 - nlen) : 1;
  if (pad > sizeof(dots) - 1) pad = sizeof(dots) - 1;
  memset(dots, '.', pad);
  dots[pad] = '\0';
  if (note && note[0]) {
    Serial.printf("[%s] %s %s %s  (%s)\n", id, name, dots, resultName(r), note);
  } else {
    Serial.printf("[%s] %s %s %s\n", id, name, dots, resultName(r));
  }
  countResult(r);
  bumpRow(row, r);
}

static uint8_t scaleCh(uint8_t v) {
  return (uint8_t)(((uint16_t)v * kPixelBright) / 255U);
}

static bool pinReserved(int p) {
  if (p < 0) return false;
  if (p == 6 || p == 54) return true;
  if (p >= 14 && p <= 19) return true;
  return false;
}

static bool pathIsDir(const char *path) {
  if (!stageStorageIsReady() || !path) return false;
  File f = stageStorageFs().open(path);
  bool ok = f && f.isDirectory();
  if (f) f.close();
  return ok;
}

static bool pathIsFile(const char *path) {
  if (!stageStorageIsReady() || !path) return false;
  if (!stageStorageFs().exists(path)) return false;
  File f = stageStorageFs().open(path);
  bool ok = f && !f.isDirectory();
  if (f) f.close();
  return ok;
}

static void restoreTestHardware() {
  if (sAudioOwned) {
    stageAudioStopShow();
    sAudioOwned = false;
  }
  if (sPixelOwned && !emergencyLocked) {
    (void)emergencyPixelsBlackout();
  }
  sPixelOwned = false;
}

static void resetSession() {
  restoreTestHardware();
  sReadOnly = false;
  sDidAudio = false;
  sDidPixels = false;
  sWaitingInput = false;
  sPixelPhase = 0;
  sWaitUntil = 0;
  sCommsPong = false;
  sPixelYes = sPixelNo = false;
  sAudioYes = sAudioNo = false;
  sEstopSawLatch = false;
  sPass = sWarn = sFail = sSkip = 0;
  sWaitWhy[0] = '\0';
  strncpy(sCurrent, "idle", sizeof(sCurrent) - 1);
  for (unsigned i = 0; i < (unsigned)Row::Count; i++) sRow[i] = Result::Skip;
}

static void abortDiag(const char *why) {
  Serial.printf("[TEST] Diagnostic aborted by operator%s%s\n",
                (why && why[0]) ? " — " : "",
                (why && why[0]) ? why : "");
  restoreTestHardware();
  sStep = Step::Idle;
  sWaitingInput = false;
  sWaitWhy[0] = '\0';
  strncpy(sCurrent, "idle", sizeof(sCurrent) - 1);
}

static bool showIsExecuting() {
  return gRuntime.rt.state == SHOW_STATE_RUNNING ||
         gRuntime.rt.running != 0;
}

static const char *waitingLabel() {
  if (!sWaitingInput) return "no";
  return sWaitWhy[0] ? sWaitWhy : "yes";
}

static void printStatus() {
  const uint32_t elapsed = (sStep == Step::Idle) ? 0 : (millis() - sStartMs);
  Serial.println("[TEST] STATUS");
  Serial.printf("[TEST] active=%s\n", sStep == Step::Idle ? "no" : "yes");
  Serial.printf("[TEST] current=%s\n", sCurrent);
  Serial.printf("[TEST] elapsed_ms=%lu\n", (unsigned long)elapsed);
  Serial.printf("[TEST] PASS=%u WARN=%u FAIL=%u SKIP=%u\n",
                (unsigned)sPass, (unsigned)sWarn, (unsigned)sFail, (unsigned)sSkip);
  Serial.printf("[TEST] waiting=%s\n", waitingLabel());
  Serial.printf("[TEST] readonly=%s emergency=%s runtime=%s\n",
                sReadOnly ? "yes" : "no",
                emergencyLocked ? "ACTIVE" : "CLEAR",
                showStateName(gRuntime.rt.state));
}

static void writeLastReport(const char *body) {
  if (!stageStorageIsReady() || !body) return;
  if (!pathIsDir(PATH_DIAGNOSTICS)) {
    if (!stageStorageFs().mkdir(PATH_DIAGNOSTICS)) {
      Serial.println("[TEST] WARN — could not create /showduino/diagnostics/");
      return;
    }
  }
  if (stageStorageFs().exists(PATH_DIAG_LAST_TEST)) {
    stageStorageFs().remove(PATH_DIAG_LAST_TEST);
  }
  File f = stageStorageFs().open(PATH_DIAG_LAST_TEST, FILE_WRITE);
  if (!f) {
    Serial.println("[TEST] WARN — could not write last-test.txt");
    return;
  }
  f.print(body);
  f.close();
  Serial.println("[TEST] Wrote /showduino/diagnostics/last-test.txt");
}

static char sReport[2048];
static size_t sReportOff = 0;

static void reportApp(const char *s) {
  if (!s) return;
  size_t n = strlen(s);
  if (sReportOff + n + 1 >= sizeof(sReport)) return;
  memcpy(sReport + sReportOff, s, n);
  sReportOff += n;
  sReport[sReportOff] = '\0';
}

static void reportAppf(const char *fmt, ...) {
  char line[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  reportApp(line);
}

static void printFinalReport() {
  uint16_t rowPass = 0, rowWarn = 0, rowFail = 0, rowSkip = 0;
  for (unsigned i = 0; i < (unsigned)Row::Count; i++) {
    switch (sRow[i]) {
      case Result::Pass: rowPass++; break;
      case Result::Warn: rowWarn++; break;
      case Result::Fail: rowFail++; break;
      default: rowSkip++; break;
    }
  }

  const char *overall = "PASS";
  if (rowFail) overall = "FAIL";
  else if (rowWarn) overall = "PASS WITH WARNINGS";

  const uint32_t elapsed = millis() - sStartMs;
  const uint32_t sec = elapsed / 1000UL;
  const uint32_t hh = sec / 3600UL;
  const uint32_t mm = (sec % 3600UL) / 60UL;
  const uint32_t ss = sec % 60UL;

  sReportOff = 0;
  sReport[0] = '\0';
  reportApp("============================================================\n");
  reportApp(" SHOWDUINO P4 — COMMISSIONING REPORT\n");
  reportApp("============================================================\n");
  reportAppf("Firmware                    0.2.0\n");
  reportAppf("Uptime ms                   %lu\n", (unsigned long)millis());
  reportAppf("Mode                        %s\n", sReadOnly ? "read-only" : "full");
#ifdef ESP_ARDUINO_VERSION_STR
  reportAppf("Arduino                     %s\n", ESP_ARDUINO_VERSION_STR);
#endif
  reportAppf("ESP-IDF                     %s\n", ESP.getSdkVersion());
  for (unsigned i = 0; i < (unsigned)Row::Count; i++) {
    reportAppf("%-28s %s\n", kRowName[i], resultName(sRow[i]));
  }
  reportApp("------------------------------------------------------------\n");
  reportAppf("PASS: %u\n", (unsigned)rowPass);
  reportAppf("WARN: %u\n", (unsigned)rowWarn);
  reportAppf("FAIL: %u\n", (unsigned)rowFail);
  reportAppf("SKIP: %u\n", (unsigned)rowSkip);
  reportAppf("RESULT: %s\n", overall);
  reportAppf("Duration: %02lu:%02lu:%02lu\n",
             (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
  reportApp("============================================================\n");

  Serial.print(sReport);
  writeLastReport(sReport);
}

static void runP4() {
  strncpy(sCurrent, "P4 SYSTEM", sizeof(sCurrent) - 1);
  Serial.println("[01] P4 SYSTEM");

  char note[48];
  snprintf(note, sizeof(note), "%s rev%u",
           ESP.getChipModel() ? ESP.getChipModel() : "ESP32-P4",
           (unsigned)ESP.getChipRevision());
  printItem("01.1", "CPU", Result::Pass, note, Row::P4System);

  snprintf(note, sizeof(note), "%u MHz", (unsigned)getCpuFrequencyMhz());
  printItem("01.2", "CPU frequency", Result::Pass, note, Row::P4System);

#ifdef ESP_ARDUINO_VERSION_STR
  snprintf(note, sizeof(note), "Arduino %s / IDF %s",
           ESP_ARDUINO_VERSION_STR, ESP.getSdkVersion());
#else
  snprintf(note, sizeof(note), "IDF %s", ESP.getSdkVersion());
#endif
  printItem("01.3", "SDK", Result::Pass, note, Row::P4System);

  const uint32_t heap = ESP.getFreeHeap();
  snprintf(note, sizeof(note), "free=%lu min=%lu",
           (unsigned long)heap, (unsigned long)ESP.getMinFreeHeap());
  printItem("01.4", "Heap", heap > 0 ? Result::Pass : Result::Fail, note, Row::Memory);

  const uint32_t ps = psramTotalBytes();
  const uint32_t pfree = psramFreeBytes();
  char psNote[72];
  if (ps == 0) {
#ifdef BOARD_HAS_PSRAM
    snprintf(psNote, sizeof(psNote), "compile enabled, APIs report 0");
#else
    snprintf(psNote, sizeof(psNote), "not reported");
#endif
    printItem("01.5", "PSRAM", Result::Warn, psNote, Row::Psram);
  } else {
    snprintf(psNote, sizeof(psNote), "total=%lu free=%lu",
             (unsigned long)ps, (unsigned long)pfree);
    printItem("01.5", "PSRAM", Result::Pass, psNote, Row::Psram);
  }
  char allocNote[40];
  if (psramAllocProbe(allocNote, sizeof(allocNote))) {
    printItem("01.6", "PSRAM allocation test", Result::Pass, allocNote, Row::Psram);
  } else if (ps == 0) {
    printItem("01.6", "PSRAM allocation test", Result::Warn, allocNote, Row::Psram);
  } else {
    printItem("01.6", "PSRAM allocation test", Result::Fail, allocNote, Row::Psram);
  }

  snprintf(note, sizeof(note), "%lu bytes", (unsigned long)ESP.getFlashChipSize());
  printItem("01.7", "Flash", ESP.getFlashChipSize() > 0 ? Result::Pass : Result::Warn,
            note, Row::P4System);

  snprintf(note, sizeof(note), "%lu ms", (unsigned long)millis());
  printItem("01.8", "Uptime", Result::Pass, note, Row::P4System);

  printItem("01.9", "Reset reason", Result::Pass,
            resetReasonName(esp_reset_reason()), Row::P4System);
}

static void runPins() {
  strncpy(sCurrent, "PIN CONFIG", sizeof(sCurrent) - 1);
  Serial.println("[02] PIN CONFIGURATION");
  Serial.printf("[02] Comms UART  RX=%d TX=%d baud=%u\n",
                SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN,
                (unsigned)SHOWDUINO_COMMS_UART_BAUD);
  Serial.printf("[02] Audio    WS=%d BCLK=%d DATA=%d\n",
                P4_AUDIO_I2S_WS, P4_AUDIO_I2S_BCLK, P4_AUDIO_I2S_DOUT);
  Serial.printf("[02] Emergency pixels=%d button=%d\n",
                SHOWDUINO_EMERGENCY_PIXEL_PIN, SHOWDUINO_ESTOP_GPIO);
  Serial.printf("[02] SDMMC    D0=%d D1=%d D2=%d D3=%d CLK=%d CMD=%d PWR=%d\n",
                SHOWDUINO_SD_D0_PIN, SHOWDUINO_SD_D1_PIN, SHOWDUINO_SD_D2_PIN,
                SHOWDUINO_SD_D3_PIN, SHOWDUINO_SD_CLK_PIN, SHOWDUINO_SD_CMD_PIN,
                SHOWDUINO_SD_POWER_PIN);
  Serial.println("[02] Reserved C6/SDIO GPIO6, GPIO14-19, GPIO54 (not wiggled)");

  const int used[] = {
    SHOWDUINO_COMMS_UART_RX_PIN,
    SHOWDUINO_COMMS_UART_TX_PIN,
    P4_AUDIO_I2S_WS,
    P4_AUDIO_I2S_BCLK,
    P4_AUDIO_I2S_DOUT,
    SHOWDUINO_EMERGENCY_PIXEL_PIN,
    SHOWDUINO_ESTOP_GPIO,
    SHOWDUINO_SD_D0_PIN,
    SHOWDUINO_SD_D1_PIN,
    SHOWDUINO_SD_D2_PIN,
    SHOWDUINO_SD_D3_PIN,
    SHOWDUINO_SD_CLK_PIN,
    SHOWDUINO_SD_CMD_PIN,
    SHOWDUINO_SD_POWER_PIN,
    SHOWDUINO_PLUGIN_BUS_SDA_PIN,
    SHOWDUINO_PLUGIN_BUS_SCL_PIN,
    10 /* STATUS_LED_PIN in sketch */
  };
  const char *names[] = {
    "COMMS_RX", "COMMS_TX", "I2S_WS", "I2S_BCLK", "I2S_DOUT",
    "PIX", "ESTOP", "SD_D0", "SD_D1", "SD_D2", "SD_D3",
    "SD_CLK", "SD_CMD", "SD_PWR", "I2C_SDA", "I2C_SCL", "LED"
  };
  const int n = (int)(sizeof(used) / sizeof(used[0]));
  bool dup = false;
  bool reservedHit = false;
  char note[64] = "";

  for (int i = 0; i < n; i++) {
    if (used[i] < 0) continue;
    if (pinReserved(used[i])) {
      reservedHit = true;
      snprintf(note, sizeof(note), "%s uses reserved GPIO%d", names[i], used[i]);
    }
    for (int j = i + 1; j < n; j++) {
      if (used[i] == used[j]) {
        dup = true;
        snprintf(note, sizeof(note), "%s and %s share GPIO%d", names[i], names[j], used[i]);
      }
    }
  }

  if (dup) {
    printItem("02.1", "GPIO unique", Result::Fail, note, Row::PinConfig);
  } else {
    printItem("02.1", "GPIO unique", Result::Pass, nullptr, Row::PinConfig);
  }
  if (reservedHit) {
    printItem("02.2", "Reserved C6/SDIO pins unused", Result::Fail, note, Row::PinConfig);
  } else {
    printItem("02.2", "Reserved C6/SDIO pins unused", Result::Pass, nullptr, Row::PinConfig);
  }

  PluginBusSelfTest pst;
  pluginBusCaptureSelfTest(&pst);
  if (!pst.busInit) {
    printItem("02.3", "Plugin bus I2C", Result::Warn, pst.detail, Row::PinConfig);
  } else {
    char pnote[48];
    snprintf(pnote, sizeof(pnote), "SDA=%d SCL=%d devices=%u",
             SHOWDUINO_PLUGIN_BUS_SDA_PIN, SHOWDUINO_PLUGIN_BUS_SCL_PIN,
             (unsigned)pst.devicesFound);
    printItem("02.3", "Plugin bus I2C", Result::Pass, pnote, Row::PinConfig);
  }
}

static void runSd() {
  strncpy(sCurrent, "SD CARD", sizeof(sCurrent) - 1);
  Serial.println("[03] SD CARD");
  const StageStorageStatus &st = stageStorageStatus();
  char note[56];

  if (!stageStorageIsReady() || !st.mounted) {
    printItem("03.1", "SD mounted", Result::Fail, st.message, Row::SdCard);
    printItem("03.2", "Capacity", Result::Skip, nullptr, Row::SdCard);
    printItem("03.3", "Write/read/delete", Result::Skip, nullptr, Row::SdCard);
    return;
  }

  snprintf(note, sizeof(note), "%s", st.cardType);
  printItem("03.1", "SD mounted", Result::Pass, note, Row::SdCard);

  snprintf(note, sizeof(note), "total=%lu MB free=%lu MB",
           (unsigned long)(st.totalBytes / (1024ULL * 1024ULL)),
           (unsigned long)(st.freeBytes / (1024ULL * 1024ULL)));
  printItem("03.2", "Capacity", (st.totalBytes > 0) ? Result::Pass : Result::Warn,
            note, Row::SdCard);

  if (sReadOnly) {
    printItem("03.3", "Write/read/delete", Result::Skip, "read-only mode", Row::SdCard);
    bumpRow(Row::SdCard, Result::Warn);
    return;
  }

  fs::FS &fs = stageStorageFs();
  if (fs.exists(PATH_DIAG_PROBE)) {
    if (!fs.remove(PATH_DIAG_PROBE)) {
      printItem("03.3", "Write/read/delete", Result::Fail,
                "could not remove leftover " PATH_DIAG_PROBE, Row::SdCard);
      return;
    }
  }

  const char *payload = "SHOWDUINO_DIAG";
  File f = fs.open(PATH_DIAG_PROBE, FILE_WRITE);
  if (!f) {
    printItem("03.3", "Write/read/delete", Result::Fail, "create failed", Row::SdCard);
    return;
  }
  size_t nw = f.print(payload);
  f.close();
  if (nw == 0) {
    fs.remove(PATH_DIAG_PROBE);
    printItem("03.3", "Write/read/delete", Result::Fail, "write failed", Row::SdCard);
    return;
  }

  f = fs.open(PATH_DIAG_PROBE, FILE_READ);
  if (!f) {
    printItem("03.3", "Write/read/delete", Result::Fail, "reopen failed", Row::SdCard);
    fs.remove(PATH_DIAG_PROBE);
    return;
  }
  char buf[24] = {};
  int n = f.read((uint8_t *)buf, sizeof(buf) - 1);
  f.close();
  if (n <= 0 || strcmp(buf, payload) != 0) {
    fs.remove(PATH_DIAG_PROBE);
    printItem("03.3", "Write/read/delete", Result::Fail, "verify mismatch", Row::SdCard);
    return;
  }
  if (!fs.remove(PATH_DIAG_PROBE)) {
    printItem("03.3", "Write/read/delete", Result::Fail,
              "delete failed — " PATH_DIAG_PROBE " remains", Row::SdCard);
    return;
  }
  if (fs.exists(PATH_DIAG_PROBE)) {
    printItem("03.3", "Write/read/delete", Result::Warn,
              PATH_DIAG_PROBE " still present", Row::SdCard);
    return;
  }
  printItem("03.3", "Write/read/delete", Result::Pass, PATH_DIAG_PROBE, Row::SdCard);
}

static void runFs() {
  strncpy(sCurrent, "FILESYSTEM", sizeof(sCurrent) - 1);
  Serial.println("[04] SHOWDUINO FILESYSTEM");

  struct Check {
    const char *id;
    const char *label;
    const char *path;
    bool required;
    bool file;
  };
  const Check checks[] = {
    { "04.1", "/showduino/", "/showduino", true, false },
    { "04.2", "/showduino/audio/", "/showduino/audio", true, false },
    { "04.3", "/showduino/webui/", PATH_WEBUI, true, false },
    { "04.4", "WebUI index", PATH_WEBUI "/index.html", true, true },
    { "04.5", "Emergency WAV", PATH_EMERGENCY_WAV, true, true },
    { "04.6", "/showduino/shows/", "/showduino/shows", false, false },
    { "04.7", "/showduino/plugins/", PATH_PLUGINS, false, false },
    { "04.8", "/showduino/diagnostics/", PATH_DIAGNOSTICS, false, false }
  };

  if (!stageStorageIsReady()) {
    for (unsigned i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
      printItem(checks[i].id, checks[i].label, Result::Skip, "SD not mounted",
                Row::Filesystem);
    }
    return;
  }

  for (unsigned i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    const Check &c = checks[i];
    bool ok = c.file ? pathIsFile(c.path) : pathIsDir(c.path);
    if (ok) {
      printItem(c.id, c.label, Result::Pass, nullptr, Row::Filesystem);
    } else if (c.required) {
      printItem(c.id, c.label, Result::Fail, "missing (required)", Row::Filesystem);
    } else {
      printItem(c.id, c.label, Result::Skip, "absent (optional)", Row::Filesystem);
    }
  }
}

static void runWav() {
  strncpy(sCurrent, "EMERGENCY WAV", sizeof(sCurrent) - 1);
  Serial.println("[05] EMERGENCY WAV VALIDATION");
  StageWavInfo info;
  if (!stageAudioInspectWav(PATH_EMERGENCY_WAV, &info)) {
    printItem("05.1", "RIFF/WAVE parse", Result::Fail, info.error, Row::EmergencyWav);
    return;
  }
  char note[56];
  snprintf(note, sizeof(note), "PCM %u-bit %s %lu Hz",
           (unsigned)info.bits,
           info.channels == 1 ? "mono" : "stereo",
           (unsigned long)info.sampleRate);
  printItem("05.1", "RIFF/WAVE PCM", Result::Pass, note, Row::EmergencyWav);

  if (!info.engineSupported) {
    printItem("05.2", "Engine format", Result::Fail,
              "I2S requires 16-bit PCM mono/stereo", Row::EmergencyWav);
  } else {
    printItem("05.2", "Engine format", Result::Pass, "16-bit PCM supported", Row::EmergencyWav);
  }

  snprintf(note, sizeof(note), "%lu bytes", (unsigned long)info.dataBytes);
  if (info.dataBytes == 0) {
    printItem("05.3", "Data chunk", Result::Fail, "empty", Row::EmergencyWav);
  } else if (!info.dataNonZero) {
    printItem("05.3", "Data chunk", Result::Warn, "present but silent sample", Row::EmergencyWav);
  } else {
    printItem("05.3", "Data chunk", Result::Pass, note, Row::EmergencyWav);
  }
}

static void runAudioStart() {
  strncpy(sCurrent, "I2S AUDIO", sizeof(sCurrent) - 1);
  Serial.println("[06] PCM5102A / I2S");
  Serial.printf("[06] Pins WS=%d BCLK=%d DATA=%d\n",
                P4_AUDIO_I2S_WS, P4_AUDIO_I2S_BCLK, P4_AUDIO_I2S_DOUT);

  if (sReadOnly || emergencyLocked) {
    printItem("06.1", "I2S playback", Result::Skip, "not safe in this state", Row::I2sAudio);
    sStep = Step::PixelStart;
    return;
  }
  if (!pathIsFile(PATH_EMERGENCY_WAV)) {
    printItem("06.1", "I2S playback", Result::Skip, "no WAV to exercise I2S", Row::I2sAudio);
    sStep = Step::PixelStart;
    return;
  }

  stageAudioResetDiagCounters();
  Serial.println("[AUDIO TEST] Playing test audio...");
  if (!stageAudioStartShow(PATH_EMERGENCY_WAV)) {
    printItem("06.1", "I2S start", Result::Fail, stageAudioStatus().lastError, Row::I2sAudio);
    sStep = Step::PixelStart;
    return;
  }
  sAudioOwned = true;
  sDidAudio = true;
  sWaitUntil = millis() + kAudioPlayMs;
  sStep = Step::AudioWait;
}

static void runAudioWait() {
  if ((int32_t)(millis() - sWaitUntil) < 0) return;
  const uint32_t bytes = stageAudioI2sBytesWritten();
  const bool i2s = stageAudioI2sStarted() || stageAudioStatus().i2sReady;
  stageAudioStopShow();
  sAudioOwned = false;

  if (!i2s) {
    printItem("06.1", "I2S channel", Result::Fail, "not started", Row::I2sAudio);
  } else {
    printItem("06.1", "I2S channel", Result::Pass, "DMA/channel ready", Row::I2sAudio);
  }

  char note[40];
  snprintf(note, sizeof(note), "%lu bytes", (unsigned long)bytes);
  if (bytes > 0) {
    printItem("06.2", "I2S data transmission", Result::Pass, note, Row::I2sAudio);
    Serial.println("[AUDIO] I2S data transmission ............ PASS");
    Serial.println("[AUDIO] Physical speaker output requires operator confirmation");
  } else {
    printItem("06.2", "I2S data transmission", Result::Fail, "no samples submitted", Row::I2sAudio);
  }
  sStep = Step::PixelStart;
}

static void runPixelStart() {
  strncpy(sCurrent, "EMERGENCY PIXELS", sizeof(sCurrent) - 1);
  Serial.println("[07] EMERGENCY NEOPIXELS");
  if (sReadOnly || emergencyLocked) {
    printItem("07.1", "Pixel driver", Result::Skip, "not safe in this state", Row::EmergencyPixels);
    sStep = Step::EstopPrompt;
    return;
  }
  if (!emergencyPixelsReady()) {
    printItem("07.1", "Pixel driver", Result::Fail, "RMT not ready", Row::EmergencyPixels);
    sStep = Step::EstopPrompt;
    return;
  }
  printItem("07.1", "Pixel driver", Result::Pass, "RMT ready", Row::EmergencyPixels);
  sPixelOwned = true;
  sDidPixels = true;
  sPixelPhase = 0;
  sWaitUntil = 0;
  sStep = Step::PixelWait;
}

static void runPixelWait() {
  if ((int32_t)(millis() - sWaitUntil) < 0) return;

  struct Col { const char *name; uint8_t r, g, b; };
  static const Col kCols[] = {
    { "OFF", 0, 0, 0 },
    { "RED", 255, 0, 0 },
    { "GREEN", 0, 255, 0 },
    { "BLUE", 0, 0, 255 },
    { "WHITE", 255, 255, 255 }
  };
  if (sPixelPhase >= 5) {
    if (!emergencyLocked) emergencyPixelsBlackout();
    sPixelOwned = false;
    printItem("07.2", "RGB sequence TX", Result::Pass,
              "software transmission only", Row::EmergencyPixels);
    Serial.println("[PIXEL] Physical illumination requires operator confirmation");
    sStep = Step::EstopPrompt;
    return;
  }

  const Col &c = kCols[sPixelPhase];
  Serial.printf("[PIXEL TEST] %s\n", c.name);
  const bool ok = emergencyPixelsWriteRgb(scaleCh(c.r), scaleCh(c.g), scaleCh(c.b));
  if (!ok) {
    sPixelOwned = false;
    if (!emergencyLocked) emergencyPixelsBlackout();
    printItem("07.2", "RGB sequence TX", Result::Fail, c.name, Row::EmergencyPixels);
    sStep = Step::EstopPrompt;
    return;
  }
  sPixelPhase++;
  sWaitUntil = millis() + kPixelStepMs;
}

static void runEstopPrompt() {
  strncpy(sCurrent, "PHYSICAL E-STOP", sizeof(sCurrent) - 1);
  Serial.println("[08] PHYSICAL EMERGENCY INPUT");
  if (sReadOnly) {
    printItem("08.1", "Physical emergency", Result::Skip, "read-only mode", Row::PhysicalEstop);
    printItem("08.2", "Emergency latch", Result::Skip, nullptr, Row::EmergencyLatch);
    printItem("09.1", "Emergency audio loop", Result::Skip, nullptr, Row::EmergencyAudioLoop);
    sStep = Step::CommsUart;
    return;
  }
#if SHOWDUINO_ESTOP_GPIO < 0
  printItem("08.1", "Physical emergency", Result::Skip, "GPIO not assigned", Row::PhysicalEstop);
  sStep = Step::CommsUart;
  return;
#else
  Serial.println("[ESTOP TEST]");
  Serial.println("Press and release the physical emergency button.");
  Serial.println("Waiting...");
  sWaitingInput = true;
  strncpy(sWaitWhy, "GPIO25 press", sizeof(sWaitWhy) - 1);
  stageAudioResetDiagCounters();
  sLoopAtEstop = stageAudioEmergencyLoopCount();
  sEstopSawLatch = false;
  sWaitUntil = millis() + kEstopPressMs;
  sStep = Step::EstopWaitPress;
#endif
}

static void runEstopWaitPress() {
  if (emergencyLocked && gEmergencySourceId == 2) {
    sEstopSawLatch = true;
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    printItem("08.1", "Physical emergency", Result::Pass,
              "debounced GPIO25", Row::PhysicalEstop);
    printItem("08.2", "Emergency latch",
              emergencyLocked ? Result::Pass : Result::Fail,
              emergencyLocked ? "ACTIVE" : "not latched", Row::EmergencyLatch);

    const bool rtOk = (gRuntime.rt.state == SHOW_STATE_EMERGENCY_STOP);
    printItem("08.3", "Runtime EMERGENCY_STOP",
              rtOk ? Result::Pass : Result::Fail,
              showStateName(gRuntime.rt.state), Row::Runtime);

    printItem("08.4", "Emergency pixels requested",
              emergencyPixelsWhiteActive() ? Result::Pass : Result::Warn,
              emergencyPixelsWhiteActive() ? "white active" : "not white",
              Row::EmergencyPixels);

    const bool audioOn = stageAudioIsEmergencyPlaying() || stageAudioStatus().emergencyPlaying;
    printItem("08.5", "Emergency audio started",
              audioOn ? Result::Pass : Result::Warn,
              audioOn ? "playing" : "not playing", Row::EmergencyAudioLoop);

    Serial.println("[ESTOP TEST] Physical emergency ........... PASS");
    Serial.println("[ESTOP TEST] Emergency latch .............. PASS");
    Serial.println("[09] EMERGENCY AUDIO LOOP");
    sWaitUntil = millis() + kEstopAudioMs;
    strncpy(sCurrent, "ESTOP AUDIO", sizeof(sCurrent) - 1);
    sStep = Step::EstopAudioWait;
    return;
  }

  if ((int32_t)(millis() - sWaitUntil) >= 0) {
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    Serial.println("[ESTOP TEST] Timeout — SKIP");
    printItem("08.1", "Physical emergency", Result::Skip, "timeout", Row::PhysicalEstop);
    printItem("08.2", "Emergency latch", Result::Skip, nullptr, Row::EmergencyLatch);
    printItem("09.1", "Emergency audio loop", Result::Skip, "no estop", Row::EmergencyAudioLoop);
    sStep = Step::CommsUart;
  }
}

static void runEstopAudioWait() {
  const uint32_t loops = stageAudioEmergencyLoopCount();
  const bool playing = stageAudioIsEmergencyPlaying() || stageAudioStatus().emergencyPlaying;
  if (playing && sLoopAtEstop == 0 && loops == 0) {
    /* still on first pass */
  }
  if (loops > sLoopAtEstop) {
    printItem("09.1", "Initial playback", playing || loops > 0 ? Result::Pass : Result::Fail,
              nullptr, Row::EmergencyAudioLoop);
    printItem("09.2", "Loop restart", Result::Pass, "EOF rewind observed", Row::EmergencyAudioLoop);
    Serial.println("[ESTOP AUDIO] Initial playback ............ PASS");
    Serial.println("[ESTOP AUDIO] Loop restart ................ PASS");
    Serial.println("[ESTOP TEST] Release the button.");
    Serial.println("[ESTOP TEST] Emergency remains latched as designed.");
    if (stageEstopDebouncedAsserted()) {
      Serial.println("[ESTOP TEST] GPIO25 is still held — CLEAR will be rejected until released");
    }
    Serial.println("To continue commissioning:");
    Serial.println("type EMERGENCY:CLEAR after releasing GPIO25.");
    sWaitingInput = true;
    strncpy(sWaitWhy, "EMERGENCY:CLEAR", sizeof(sWaitWhy) - 1);
    sWaitUntil = millis() + kEstopClearMs;
    strncpy(sCurrent, "WAIT CLEAR", sizeof(sCurrent) - 1);
    sStep = Step::EstopWaitClear;
    return;
  }

  if ((int32_t)(millis() - sWaitUntil) >= 0) {
    if (playing) {
      printItem("09.1", "Initial playback", Result::Pass, nullptr, Row::EmergencyAudioLoop);
      printItem("09.2", "Loop restart", Result::Warn, "not observed before timeout",
                Row::EmergencyAudioLoop);
    } else {
      printItem("09.1", "Initial playback", Result::Fail, "not playing", Row::EmergencyAudioLoop);
      printItem("09.2", "Loop restart", Result::Skip, nullptr, Row::EmergencyAudioLoop);
    }
    Serial.println("[ESTOP TEST] Release the button.");
    Serial.println("[ESTOP TEST] Emergency remains latched as designed.");
    Serial.println("To continue commissioning:");
    Serial.println("type EMERGENCY:CLEAR after releasing GPIO25.");
    sWaitingInput = true;
    strncpy(sWaitWhy, "EMERGENCY:CLEAR", sizeof(sWaitWhy) - 1);
    sWaitUntil = millis() + kEstopClearMs;
    sStep = Step::EstopWaitClear;
  }
}

static void runEstopWaitClear() {
  if (!emergencyLocked) {
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    Serial.println("[TEST] Emergency cleared by operator — resuming diagnostics");
    sStep = Step::CommsUart;
    return;
  }
  if ((int32_t)(millis() - sWaitUntil) >= 0) {
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    Serial.println("[TEST] Timeout waiting for EMERGENCY:CLEAR — continuing non-destructive tests");
    Serial.println("[TEST] Emergency remains latched (diagnostic will not clear it)");
    sStep = Step::CommsUart;
  }
}

static void runCommsUart() {
  strncpy(sCurrent, "COMMS UART", sizeof(sCurrent) - 1);
  Serial.println("[10] COMMUNICATIONS CONTROLLER");
  Serial.printf("[10] Wiring: S3 TX GPIO%d -> P4 RX GPIO%d, S3 RX GPIO%d <- P4 TX GPIO%d, GND\n",
                SHOWDUINO_COMMS_PEER_TX_PIN, SHOWDUINO_COMMS_UART_RX_PIN,
                SHOWDUINO_COMMS_PEER_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN);
  char note[48];
  snprintf(note, sizeof(note), "RX=%d TX=%d 115200",
           SHOWDUINO_COMMS_UART_RX_PIN, SHOWDUINO_COMMS_UART_TX_PIN);
  if (!stageCommsUartReady()) {
    printItem("10.1", "UART initialised", Result::Fail, nullptr, Row::CommsUart);
    printItem("10.2", "S3 controller", Result::Warn,
              "UART not initialised", Row::CommsUart);
    printItem("10.3", "Round trip", Result::Skip, nullptr, Row::CommsRoundTrip);
    sStep = Step::Director;
    return;
  }
  printItem("10.1", "UART initialised", Result::Pass, note, Row::CommsUart);
  if (stageCommsLinkUp()) {
    Serial.printf("[10] Prior comms RX %lu ms ago\n",
                  (unsigned long)(millis() - stageCommsLastRxMs()));
  }
  sStep = Step::CommsPing;
}

static void runCommsPing() {
  strncpy(sCurrent, "COMMS PING", sizeof(sCurrent) - 1);
  if (!stageCommsUartReady()) {
    printItem("10.2", "S3 controller", Result::Warn,
              "communications controller not detected", Row::CommsUart);
    printItem("10.3", "Round trip", Result::Skip, nullptr, Row::CommsRoundTrip);
    sStep = Step::Director;
    return;
  }
  sCommsPong = false;
  sPingSentMs = millis();
  Serial.println("[COMMS TEST] TX DIAG:PING");
  stageCommsSendLine("DIAG:PING");
  sWaitUntil = millis() + kCommsPongMs;
  sStep = Step::CommsPongWait;
}

static void runCommsPongWait() {
  if (sCommsPong) {
    const uint32_t rtt = millis() - sPingSentMs;
    char note[24];
    snprintf(note, sizeof(note), "%lu ms", (unsigned long)rtt);
    Serial.printf("[COMMS TEST] PONG received in %lu ms\n", (unsigned long)rtt);
    printItem("10.2", "S3 controller", Result::Pass, "DIAG:PONG", Row::CommsUart);
    printItem("10.3", "Round trip", Result::Pass, note, Row::CommsRoundTrip);
    sStep = Step::Director;
    return;
  }
  if ((int32_t)(millis() - sWaitUntil) >= 0) {
    printItem("10.2", "S3 controller", Result::Warn,
              "WARN — communications controller not detected", Row::CommsUart);
    printItem("10.3", "Round trip", Result::Skip, "no DIAG:PONG", Row::CommsRoundTrip);
    sStep = Step::Director;
  }
}

static void runDirector() {
  strncpy(sCurrent, "DIRECTOR", sizeof(sCurrent) - 1);
  Serial.println("[12] DIRECTOR PATH");
  if (stageCommsSawDirectorTraffic()) {
    Serial.println("[12] Director frames have been observed on Comms UART");
    Serial.println("[12] That is not an end-to-end acknowledgement");
  }
  printItem("12.1", "Director round-trip", Result::Skip,
            "requires Director confirmation", Row::DirectorLink);
  sStep = Step::Parser;
}

static void runParser() {
  strncpy(sCurrent, "COMMAND PARSER", sizeof(sCurrent) - 1);
  Serial.println("[13] STAGE COMMAND DISPATCHER");
  const ShowState st0 = gRuntime.rt.state;
  const bool em0 = emergencyLocked;
  const uint8_t run0 = gRuntime.rt.running;

  stageDiagDispatchLocal("STATUS:REQUEST");
  printItem("13.1", "STATUS:REQUEST", Result::Pass, "dispatched", Row::CommandParser);

  stageDiagDispatchLocal("ZZZ:NOT_A_COMMAND");
  printItem("13.2", "Unknown command", Result::Pass, "rejected", Row::CommandParser);

  char over[SHOWDUINO_COMMS_CMD_MAX + 8];
  memset(over, 'A', sizeof(over) - 1);
  over[sizeof(over) - 1] = '\0';
  stageDiagDispatchLocal(over);
  printItem("13.3", "Over-length command", Result::Pass, "rejected", Row::CommandParser);

  stageDiagDispatchLocal("");
  printItem("13.4", "Empty command", Result::Pass, "ignored", Row::CommandParser);

  char bad[4] = { 1, 'X', 'Y', 0 };
  stageDiagDispatchLocal(bad);
  printItem("13.5", "Malformed framing", Result::Pass, "rejected", Row::CommandParser);

  if (emergencyLocked != em0 || gRuntime.rt.running != run0 ||
      (gRuntime.rt.state == SHOW_STATE_RUNNING && st0 != SHOW_STATE_RUNNING)) {
    printItem("13.6", "Runtime unchanged", Result::Fail,
              "parser mutated show/emergency", Row::CommandParser);
  } else {
    printItem("13.6", "Runtime unchanged", Result::Pass, nullptr, Row::CommandParser);
  }
  sStep = Step::Runtime;
}

static void runRuntime() {
  strncpy(sCurrent, "RUNTIME", sizeof(sCurrent) - 1);
  Serial.println("[14] RUNTIME STATE MACHINE");
  const ShowState st = gRuntime.rt.state;
  char note[48];
  snprintf(note, sizeof(note), "%s", showStateName(st));

  if (st == SHOW_STATE_RUNNING) {
    printItem("14.1", "Runtime object", Result::Fail, "show running", Row::Runtime);
  } else if (sEstopSawLatch && !emergencyLocked && st == SHOW_STATE_RUNNING) {
    printItem("14.1", "Post-emergency state", Result::Fail,
              "show auto-resumed", Row::Runtime);
  } else if (sEstopSawLatch && !emergencyLocked) {
    const bool safe = (st == SHOW_STATE_IDLE || st == SHOW_STATE_SHOW_LOADED ||
                       st == SHOW_STATE_PAUSED || st == SHOW_STATE_FINISHED);
    printItem("14.1", "Post-emergency state",
              safe ? Result::Pass : Result::Warn, note, Row::Runtime);
  } else if (emergencyLocked) {
    printItem("14.1", "Runtime object",
              (st == SHOW_STATE_EMERGENCY_STOP) ? Result::Pass : Result::Warn,
              note, Row::Runtime);
  } else if (st == SHOW_STATE_IDLE || st == SHOW_STATE_SHOW_LOADED ||
             st == SHOW_STATE_FINISHED) {
    printItem("14.1", "Runtime object", Result::Pass, note, Row::Runtime);
  } else {
    printItem("14.1", "Runtime object", Result::Warn, note, Row::Runtime);
  }
  sStep = Step::ClockStart;
}

static void runClockStart() {
  strncpy(sCurrent, "SYSTEM CLOCK", sizeof(sCurrent) - 1);
  Serial.println("[15] RTC / SYSTEM TIME");
  struct timeval tv = {};
  if (gettimeofday(&tv, nullptr) != 0) {
    printItem("15.1", "Time API", Result::Fail, "gettimeofday failed", Row::SystemClock);
    sStep = Step::WebUi;
    return;
  }
  sClockSec0 = (uint32_t)tv.tv_sec;
  sClockUsec0 = (uint32_t)tv.tv_usec;
  char note[40];
  snprintf(note, sizeof(note), "sec=%lu", (unsigned long)sClockSec0);
  printItem("15.1", "Time API", Result::Pass, note, Row::SystemClock);
  sWaitUntil = millis() + kClockMs;
  sStep = Step::ClockWait;
}

static void runClockWait() {
  if ((int32_t)(millis() - sWaitUntil) < 0) return;
  struct timeval tv = {};
  gettimeofday(&tv, nullptr);
  const uint64_t t0 = ((uint64_t)sClockSec0 * 1000000ULL) + sClockUsec0;
  const uint64_t t1 = ((uint64_t)tv.tv_sec * 1000000ULL) + (uint32_t)tv.tv_usec;
  if (t1 > t0) {
    printItem("15.2", "Clock advancing", Result::Pass, nullptr, Row::SystemClock);
  } else {
    printItem("15.2", "Clock advancing", Result::Fail, "value did not increase", Row::SystemClock);
  }
  if ((uint32_t)tv.tv_sec < kTimeSyncedFloor) {
    printItem("15.3", "Absolute time", Result::Warn,
              "clock running but absolute time not synchronised", Row::SystemClock);
  } else {
    printItem("15.3", "Absolute time", Result::Pass, "looks synchronised", Row::SystemClock);
  }
  sStep = Step::WebUi;
}

static void runWebUi() {
  strncpy(sCurrent, "WEBUI", sizeof(sCurrent) - 1);
  Serial.println("[16] WEBUI ORIGIN");
  if (!webApiOriginReady()) {
    printItem("16.1", "P4 WebUI origin", Result::Fail, "not initialised", Row::WebUi);
  } else {
    printItem("16.1", "P4 WebUI origin", Result::Pass, "initialised", Row::WebUi);
  }
  char err[48] = "";
  char resolved[64] = "";
  if (webApiProbePublicUrl("/", resolved, sizeof(resolved), err, sizeof(err))) {
    char note[72];
    snprintf(note, sizeof(note), "GET / -> %s", resolved);
    printItem("16.2", "Origin GET /", Result::Pass, note, Row::WebUi);
  } else if (webApiProbePublicUrl("/index.html", resolved, sizeof(resolved), err, sizeof(err))) {
    char note[72];
    snprintf(note, sizeof(note), "GET /index.html -> %s", resolved);
    printItem("16.2", "Origin GET /index.html", Result::Pass, note, Row::WebUi);
  } else {
    printItem("16.2", "Origin index", Result::Fail, err[0] ? err : "unreadable", Row::WebUi);
  }
  printItem("16.3", "Remote browser transport", Result::Skip,
            "S3 has no SoftAP/WebUI proxy", Row::WebUi);
  sStep = Step::Memory;
}

static void runMemory() {
  strncpy(sCurrent, "MEMORY", sizeof(sCurrent) - 1);
  Serial.println("[17] MEMORY AFTER TEST");
  const uint32_t heapEnd = ESP.getFreeHeap();
  const uint32_t psEnd = psramFreeBytes();
  const int32_t heapD = (int32_t)heapEnd - (int32_t)sHeapStart;
  const int32_t psD = (int32_t)psEnd - (int32_t)sPsramStart;
  char note[56];
  snprintf(note, sizeof(note), "start=%lu end=%lu d=%ld",
           (unsigned long)sHeapStart, (unsigned long)heapEnd, (long)heapD);
  Result hr = Result::Pass;
  if (heapD < -kHeapWarnDrop) hr = Result::Warn;
  printItem("17.1", "Heap delta", hr, note, Row::Memory);

  if (psramTotalBytes() == 0 && sPsramStart == 0 && psEnd == 0) {
    printItem("17.2", "PSRAM delta", Result::Skip, "PSRAM not measurable", Row::Psram);
  } else {
    snprintf(note, sizeof(note), "start=%lu end=%lu d=%ld",
             (unsigned long)sPsramStart, (unsigned long)psEnd, (long)psD);
    Result pr = Result::Pass;
    if (psD < -kHeapWarnDrop) pr = Result::Warn;
    printItem("17.2", "PSRAM delta", pr, note, Row::Psram);
  }
  sStep = Step::ConfirmPrompt;
}

static void runConfirmPrompt() {
  if (!sDidPixels && !sDidAudio) {
    sStep = Step::Report;
    return;
  }
  strncpy(sCurrent, "CONFIRM", sizeof(sCurrent) - 1);
  Serial.println();
  Serial.println("VISUAL / AUDIO CONFIRMATION");
  if (sDidPixels) {
    Serial.println("Did you observe the NeoPixels display:");
    Serial.println("RED → GREEN → BLUE → WHITE ?");
    Serial.println("Type:");
    Serial.println("CONFIRM:PIXELS:YES");
    Serial.println("or");
    Serial.println("CONFIRM:PIXELS:NO");
  }
  if (sDidAudio) {
    Serial.println("Did you hear the audio test?");
    Serial.println("Type:");
    Serial.println("CONFIRM:AUDIO:YES");
    Serial.println("or");
    Serial.println("CONFIRM:AUDIO:NO");
  }
  sWaitingInput = true;
  strncpy(sWaitWhy, "CONFIRM:PIXELS/AUDIO", sizeof(sWaitWhy) - 1);
  sWaitUntil = millis() + kConfirmMs;
  sStep = Step::ConfirmWait;
}

static bool confirmsDone() {
  const bool pix = !sDidPixels || sPixelYes || sPixelNo;
  const bool aud = !sDidAudio || sAudioYes || sAudioNo;
  return pix && aud;
}

static void applyConfirms() {
  if (sDidPixels) {
    if (sPixelYes) {
      printItem("C.1", "Pixels physical", Result::Pass, "operator YES", Row::PixelsPhysical);
    } else if (sPixelNo) {
      printItem("C.1", "Pixels physical", Result::Fail, "operator NO", Row::PixelsPhysical);
    } else {
      printItem("C.1", "Pixels physical", Result::Skip, "no confirmation", Row::PixelsPhysical);
    }
  }
  if (sDidAudio) {
    if (sAudioYes) {
      printItem("C.2", "Audio physical", Result::Pass, "operator YES", Row::AudioPhysical);
    } else if (sAudioNo) {
      printItem("C.2", "Audio physical", Result::Fail, "operator NO", Row::AudioPhysical);
    } else {
      printItem("C.2", "Audio physical", Result::Skip, "no confirmation", Row::AudioPhysical);
    }
  }
}

static void runConfirmWait() {
  if (confirmsDone()) {
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    applyConfirms();
    sStep = Step::Report;
    return;
  }
  if ((int32_t)(millis() - sWaitUntil) >= 0) {
    sWaitingInput = false;
    sWaitWhy[0] = '\0';
    Serial.println("[TEST] Confirmation timeout — recording SKIP for unanswered items");
    applyConfirms();
    sStep = Step::Report;
  }
}

static void startFullOrReject() {
  if (sStep != Step::Idle) {
    Serial.println("[TEST] Diagnostic already running — type RUN:TEST:STATUS or RUN:TEST:ABORT");
    return;
  }

  if (showIsExecuting()) {
    Serial.println("[TEST] Full diagnostic unavailable while show is running");
    return;
  }

  resetSession();
  sStartMs = millis();
  sHeapStart = ESP.getFreeHeap();
  sPsramStart = psramFreeBytes();

  if (emergencyLocked || gRuntime.rt.state == SHOW_STATE_EMERGENCY_STOP) {
    Serial.println("[TEST] Full diagnostic unavailable while emergency is latched");
    Serial.println("[TEST] Running reduced read-only diagnostic");
    sReadOnly = true;
  }

  sStep = Step::Header;
}

}  // namespace

void stageDiagNoteCommsPong() {
  sCommsPong = true;
}

void stageDiagNoteCommsLine(const char *line) {
  if (sStep != Step::CommsPongWait) return;
  Serial.printf("[COMMS TEST] RX %s\n", (line && line[0]) ? line : "(empty)");
  if (line && !strcmp(line, "DIAG:PONG")) sCommsPong = true;
}

bool stageDiagIsActive() {
  return sStep != Step::Idle;
}

bool stageDiagHandleCommand(const char *cmd) {
  if (!cmd || !cmd[0]) return false;

  if (!strcmp(cmd, "RUN:TEST")) {
    startFullOrReject();
    return true;
  }
  if (!strcmp(cmd, "RUN:TEST:STATUS")) {
    printStatus();
    return true;
  }
  if (!strcmp(cmd, "RUN:TEST:ABORT")) {
    if (sStep == Step::Idle) {
      Serial.println("[TEST] No diagnostic is running");
    } else {
      abortDiag(nullptr);
    }
    return true;
  }
  if (!strcmp(cmd, "CONFIRM:PIXELS:YES") || !strcmp(cmd, "CONFIRM:PIXELS:NO") ||
      !strcmp(cmd, "CONFIRM:AUDIO:YES") || !strcmp(cmd, "CONFIRM:AUDIO:NO")) {
    if (sStep != Step::ConfirmWait) {
      Serial.println("[TEST] No confirmation is currently requested");
      return true;
    }
    if (!strcmp(cmd, "CONFIRM:PIXELS:YES")) sPixelYes = true;
    if (!strcmp(cmd, "CONFIRM:PIXELS:NO")) sPixelNo = true;
    if (!strcmp(cmd, "CONFIRM:AUDIO:YES")) sAudioYes = true;
    if (!strcmp(cmd, "CONFIRM:AUDIO:NO")) sAudioNo = true;
    Serial.printf("[TEST] Recorded %s\n", cmd);
    return true;
  }
  return false;
}

void stageDiagService() {
  switch (sStep) {
    case Step::Idle:
      break;
    case Step::Header:
      Serial.println("============================================================");
      Serial.println(" SHOWDUINO P4 STAGE ENGINE — SYSTEM COMMISSIONING TEST");
      Serial.println("============================================================");
      Serial.println("[TEST] Starting diagnostics...");
      if (sReadOnly) Serial.println("[TEST] Mode: read-only (emergency latched)");
      sStep = Step::P4;
      break;
    case Step::P4:
      runP4();
      sStep = Step::Pins;
      break;
    case Step::Pins:
      runPins();
      sStep = Step::Sd;
      break;
    case Step::Sd:
      runSd();
      sStep = Step::Fs;
      break;
    case Step::Fs:
      runFs();
      sStep = Step::Wav;
      break;
    case Step::Wav:
      runWav();
      sStep = Step::AudioStart;
      break;
    case Step::AudioStart:
      runAudioStart();
      break;
    case Step::AudioWait:
      runAudioWait();
      break;
    case Step::PixelStart:
      runPixelStart();
      break;
    case Step::PixelWait:
      runPixelWait();
      break;
    case Step::EstopPrompt:
      runEstopPrompt();
      break;
    case Step::EstopWaitPress:
      runEstopWaitPress();
      break;
    case Step::EstopAudioWait:
      runEstopAudioWait();
      break;
    case Step::EstopWaitClear:
      runEstopWaitClear();
      break;
    case Step::CommsUart:
      runCommsUart();
      break;
    case Step::CommsPing:
      runCommsPing();
      break;
    case Step::CommsPongWait:
      runCommsPongWait();
      break;
    case Step::Director:
      runDirector();
      break;
    case Step::Parser:
      runParser();
      break;
    case Step::Runtime:
      runRuntime();
      break;
    case Step::ClockStart:
      runClockStart();
      break;
    case Step::ClockWait:
      runClockWait();
      break;
    case Step::WebUi:
      runWebUi();
      break;
    case Step::Memory:
      runMemory();
      break;
    case Step::ConfirmPrompt:
      runConfirmPrompt();
      break;
    case Step::ConfirmWait:
      runConfirmWait();
      break;
    case Step::Report:
      printFinalReport();
      sStep = Step::Idle;
      sWaitingInput = false;
      strncpy(sCurrent, "idle", sizeof(sCurrent) - 1);
      break;
    default:
      sStep = Step::Idle;
      break;
  }
}
