#include "CommsStatusRgb.h"
#include "../EspNowTransport.h"
#include "../ProtocolBridge.h"
#include "../web/CommsWebServer.h"
#include "../../BoardConfig.h"

#if SHOWDUINO_COMMS_RGB_COUNT != 1
#error "Comms status RGB supports one onboard LED"
#endif

struct RgbColour {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static CommsRgbState sState = CommsRgbState::Boot;
static CommsRgbState sLogged = CommsRgbState::Boot;
static bool sWebStarting = false;
static bool sBootHold = true;
static bool sInited = false;
static uint32_t sWebReadyMs = 0;
static uint32_t sBothOnlineMs = 0;
static bool sBothOnlineLatch = false;
static uint32_t sTestUntil = 0;
static uint8_t sTestIndex = 0;
static uint32_t sTestStepAt = 0;
static uint8_t sLastR = 255;
static uint8_t sLastG = 255;
static uint8_t sLastB = 255;

static const CommsRgbState kTestOrder[] = {
  CommsRgbState::Boot,
  CommsRgbState::WebuiStarting,
  CommsRgbState::WebuiReady,
  CommsRgbState::Synchronising,
  CommsRgbState::Healthy,
  CommsRgbState::DirectorOffline,
  CommsRgbState::P4Offline,
  CommsRgbState::WebuiFault,
  CommsRgbState::EspNowFault,
  CommsRgbState::Emergency,
  CommsRgbState::Fault
};

static const char *stateName(CommsRgbState state) {
  switch (state) {
    case CommsRgbState::Boot: return "BOOT";
    case CommsRgbState::WebuiStarting: return "WEBUI_STARTING";
    case CommsRgbState::WebuiReady: return "WEBUI_READY";
    case CommsRgbState::Synchronising: return "SYNCHRONISING";
    case CommsRgbState::Healthy: return "HEALTHY";
    case CommsRgbState::DirectorOffline: return "DIRECTOR_OFFLINE";
    case CommsRgbState::P4Offline: return "P4_OFFLINE";
    case CommsRgbState::WebuiFault: return "WEBUI_FAULT";
    case CommsRgbState::EspNowFault: return "ESPNOW_FAULT";
    case CommsRgbState::Emergency: return "EMERGENCY";
    case CommsRgbState::Fault: return "FAULT";
  }
  return "FAULT";
}

static const char *colourName(CommsRgbState state) {
  switch (state) {
    case CommsRgbState::Boot: return "PURPLE";
    case CommsRgbState::WebuiStarting: return "BLUE";
    case CommsRgbState::WebuiReady: return "BLUE";
    case CommsRgbState::Synchronising: return "CYAN";
    case CommsRgbState::Healthy: return "GREEN";
    case CommsRgbState::DirectorOffline: return "AMBER";
    case CommsRgbState::P4Offline: return "AMBER";
    case CommsRgbState::WebuiFault: return "BLUE";
    case CommsRgbState::EspNowFault: return "MAGENTA";
    case CommsRgbState::Emergency: return "RED";
    case CommsRgbState::Fault: return "RED/MAGENTA";
  }
  return "RED/MAGENTA";
}

static uint8_t scale(uint8_t value) {
  return (uint8_t)(((uint16_t)value * SHOWDUINO_COMMS_RGB_BRIGHTNESS) / 255U);
}

static void writeRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (r == sLastR && g == sLastG && b == sLastB) return;
  sLastR = r;
  sLastG = g;
  sLastB = b;
#if defined(rgbLedWrite)
  rgbLedWrite(SHOWDUINO_COMMS_RGB_PIN, scale(r), scale(g), scale(b));
#else
  neopixelWrite(SHOWDUINO_COMMS_RGB_PIN, scale(r), scale(g), scale(b));
#endif
}

static uint8_t breathe(uint32_t now, uint32_t periodMs) {
  const uint32_t t = now % periodMs;
  const uint32_t half = periodMs / 2UL;
  if (t <= half) return (uint8_t)((t * 255UL) / half);
  return (uint8_t)(((periodMs - t) * 255UL) / half);
}

static void mix(RgbColour base, uint8_t level) {
  writeRgb((uint8_t)((base.r * level) / 255U),
           (uint8_t)((base.g * level) / 255U),
           (uint8_t)((base.b * level) / 255U));
}

static void render(CommsRgbState state, uint32_t now) {
  const RgbColour purple = {160, 0, 220};
  const RgbColour blue = {0, 40, 255};
  const RgbColour cyan = {0, 200, 220};
  const RgbColour green = {0, 255, 40};
  const RgbColour amber = {255, 140, 0};
  const RgbColour magenta = {220, 0, 180};
  const RgbColour red = {255, 0, 0};

  switch (state) {
    case CommsRgbState::Boot:
      mix(purple, breathe(now, 1600));
      break;
    case CommsRgbState::WebuiStarting:
      mix(blue, breathe(now, 1400));
      break;
    case CommsRgbState::WebuiReady:
      writeRgb(blue.r, blue.g, blue.b);
      break;
    case CommsRgbState::Synchronising:
      mix(cyan, breathe(now, 1400));
      break;
    case CommsRgbState::Healthy:
      writeRgb(green.r, green.g, green.b);
      break;
    case CommsRgbState::DirectorOffline:
      mix(amber, breathe(now, 1400));
      break;
    case CommsRgbState::P4Offline: {
      const uint32_t t = now % 840UL;
      const bool on = (t < 80UL) || (t >= 160UL && t < 240UL);
      if (on) writeRgb(amber.r, amber.g, amber.b);
      else writeRgb(0, 0, 0);
      break;
    }
    case CommsRgbState::WebuiFault: {
      const bool on = ((now / 120UL) % 2UL) == 0UL;
      if (on) writeRgb(blue.r, blue.g, blue.b);
      else writeRgb(0, 0, 0);
      break;
    }
    case CommsRgbState::EspNowFault: {
      const bool on = ((now / 120UL) % 2UL) == 0UL;
      if (on) writeRgb(magenta.r, magenta.g, magenta.b);
      else writeRgb(0, 0, 0);
      break;
    }
    case CommsRgbState::Emergency:
      mix(red, breathe(now, 280));
      break;
    case CommsRgbState::Fault: {
      const bool redPhase = ((now / 250UL) % 2UL) == 0UL;
      if (redPhase) writeRgb(red.r, red.g, red.b);
      else writeRgb(magenta.r, magenta.g, magenta.b);
      break;
    }
  }
}

static void logIfChanged(CommsRgbState state) {
  if (state == sLogged) return;
  sLogged = state;
  Serial.printf("[RGB] %s\n", stateName(state));
}

static CommsRgbState resolveLive(uint32_t now) {
  if (sBootHold) return CommsRgbState::Boot;

  const bool webReady = commsWebReady();
  const bool webFault = commsWebFault();
  const bool espNowOk = espNowTransportReady();
  const bool directorOnline = protocolBridgeDirectorOnline();
  const bool directorSeen = espNowTransportHaveDirector();
  const bool p4Online = protocolBridgeP4Alive();
  const bool emergency = protocolBridgeEmergencyActive();

  if (emergency) return CommsRgbState::Emergency;
  if (!espNowOk) return CommsRgbState::EspNowFault;
  if (webFault) return CommsRgbState::WebuiFault;

  if (sWebStarting && !webReady && !webFault) return CommsRgbState::WebuiStarting;

  if (webReady && sWebReadyMs == 0) sWebReadyMs = now;
  const bool pastGrace = webReady &&
                         sWebReadyMs != 0 &&
                         (now - sWebReadyMs) >= SHOWDUINO_COMMS_RGB_GRACE_MS;

  if (p4Online && directorOnline) {
    if (!sBothOnlineLatch) {
      sBothOnlineLatch = true;
      sBothOnlineMs = now;
    }
    if ((now - sBothOnlineMs) < SHOWDUINO_COMMS_RGB_SYNC_MS) {
      return CommsRgbState::Synchronising;
    }
    return CommsRgbState::Healthy;
  }
  sBothOnlineLatch = false;

  if (pastGrace && !p4Online) return CommsRgbState::P4Offline;
  if (pastGrace && p4Online && directorSeen && !directorOnline) {
    return CommsRgbState::DirectorOffline;
  }
  if (pastGrace && p4Online && !directorSeen) return CommsRgbState::DirectorOffline;

  if (webReady) return CommsRgbState::WebuiReady;
  if (sWebStarting) return CommsRgbState::WebuiStarting;
  return CommsRgbState::Boot;
}

void commsStatusRgbBegin() {
  sState = CommsRgbState::Boot;
  sLogged = CommsRgbState::Boot;
  sWebStarting = false;
  sBootHold = true;
  sWebReadyMs = 0;
  sBothOnlineMs = 0;
  sBothOnlineLatch = false;
  sTestUntil = 0;
  sLastR = 255;
  sLastG = 255;
  sLastB = 255;
  sInited = true;
  writeRgb(160, 0, 220);
  Serial.println("[RGB] BOOT");
}

void commsStatusRgbNoteWebStarting() {
  sBootHold = false;
  sWebStarting = true;
}

void commsStatusRgbStartTest() {
  sTestIndex = 0;
  sTestStepAt = millis();
  sTestUntil = millis() + (uint32_t)(sizeof(kTestOrder) / sizeof(kTestOrder[0])) * 900UL;
  Serial.println("[RGB] TEST begin — presentation only, system state unchanged");
}

void commsStatusRgbLoop() {
  if (!sInited) return;
  const uint32_t now = millis();
  CommsRgbState next = resolveLive(now);

  if (sTestUntil != 0 && (int32_t)(now - sTestUntil) < 0) {
    if ((now - sTestStepAt) >= 900UL) {
      sTestStepAt = now;
      if (sTestIndex + 1 < (uint8_t)(sizeof(kTestOrder) / sizeof(kTestOrder[0]))) {
        sTestIndex++;
      }
    }
    next = kTestOrder[sTestIndex];
  } else if (sTestUntil != 0) {
    sTestUntil = 0;
    Serial.println("[RGB] TEST complete — returning to live status");
    next = resolveLive(now);
  }

  sState = next;
  logIfChanged(sState);
  render(sState, now);
}

void commsStatusRgbPrintStatus() {
  Serial.println("[RGB]");
  Serial.printf("Pin: %u\n", (unsigned)SHOWDUINO_COMMS_RGB_PIN);
  Serial.printf("State: %s\n", commsStatusRgbStateName());
  Serial.printf("Colour: %s\n", commsStatusRgbColourName());
  Serial.printf("Brightness: %u\n", (unsigned)SHOWDUINO_COMMS_RGB_BRIGHTNESS);
  Serial.printf("WebUI: %s\n", commsWebReady() ? "ONLINE" : (commsWebFault() ? "FAULT" : "OFFLINE"));
  Serial.printf("ESP-NOW: %s\n", espNowTransportReady() ? "ONLINE" : "FAULT");
  Serial.printf("Director: %s\n", protocolBridgeDirectorOnline() ? "ONLINE" : "OFFLINE");
  Serial.printf("P4: %s\n", protocolBridgeP4Alive() ? "ONLINE" : "OFFLINE");
  Serial.printf("Emergency: %s\n", protocolBridgeEmergencyActive() ? "ACTIVE" : "CLEAR");
}

CommsRgbState commsStatusRgbState() { return sState; }
const char *commsStatusRgbStateName() { return stateName(sState); }
const char *commsStatusRgbColourName() { return colourName(sState); }
uint8_t commsStatusRgbBrightness() { return SHOWDUINO_COMMS_RGB_BRIGHTNESS; }
uint8_t commsStatusRgbPin() { return SHOWDUINO_COMMS_RGB_PIN; }
