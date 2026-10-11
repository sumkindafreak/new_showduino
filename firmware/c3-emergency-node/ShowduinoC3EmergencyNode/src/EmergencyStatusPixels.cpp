#include "EmergencyStatusPixels.h"
#include "../BoardConfig.h"

#include <Adafruit_NeoPixel.h>

/*
 * Local indicator only. Two pixels:
 *   0: radio / station health
 *   1: low-level atmosphere
 *
 * Never receives theatrical PIXEL:* commands, and cannot assert/clear
 * any safety state. Indicators always follow the actual safety state.
 */
static Adafruit_NeoPixel sPixels(SHOWDUINO_ESTOP_STATUS_PIXEL_COUNT,
                                SHOWDUINO_ESTOP_STATUS_PIXEL_PIN,
                                SHOWDUINO_PIXEL_ORDER);
static bool sReady = false;
static uint8_t sLastMode = 0xFF;
static uint32_t sLastFrame = 0;
static const char *sModeName = "BOOT";

enum StatusMode : uint8_t {
  MODE_EMERGENCY = 0,
  MODE_FAULT,
  MODE_LOCATE,
  MODE_SEARCHING,
  MODE_BOOT,
  MODE_READY
};

static uint8_t dim(uint8_t value, uint8_t brightness) {
  return (uint8_t)(((uint16_t)value * brightness) / 255U);
}

static void rgb(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b,
                uint8_t brightness) {
  sPixels.setPixelColor(pixel, dim(r, brightness),
                       dim(g, brightness), dim(b, brightness));
}

void emergencyStatusPixelsSafeGpio() {
  /* Hold data LOW before the radio, OLED or NeoPixel driver begins. */
  pinMode(SHOWDUINO_ESTOP_STATUS_PIXEL_PIN, OUTPUT);
  digitalWrite(SHOWDUINO_ESTOP_STATUS_PIXEL_PIN, LOW);
}

void emergencyStatusPixelsBegin() {
  /* No .show() here: emergency protocol must be serviced first. */
  sPixels.begin();
  sReady = true;
  sLastMode = 0xFF;
  sLastFrame = 0;
  Serial.printf("[ESTOP-STATUS-PIX] GPIO%d, fixed %u pixels, independent of GPIO%d show line\n",
                SHOWDUINO_ESTOP_STATUS_PIXEL_PIN,
                (unsigned)SHOWDUINO_ESTOP_STATUS_PIXEL_COUNT,
                SHOWDUINO_ESTOP_PIXEL_DATA_PIN);
}

void emergencyStatusPixelsService(bool latched, bool systemEmergency,
                                  bool buttonPressed, bool radioFresh,
                                  ShowduinoEmergencyNodeState state,
                                  bool locateActive) {
  if (!sReady) return;
  const uint32_t now = millis();
  StatusMode mode;
  if (latched || systemEmergency || buttonPressed) {
    mode = MODE_EMERGENCY;
  } else if (state == SHOWDUINO_ESTOP_ST_FAULT ||
             state == SHOWDUINO_ESTOP_ST_NEEDS_REARM) {
    mode = MODE_FAULT;
  } else if (locateActive) {
    mode = MODE_LOCATE;
  } else if (state == SHOWDUINO_ESTOP_ST_BOOTING) {
    mode = MODE_BOOT;
  } else if (!radioFresh || state == SHOWDUINO_ESTOP_ST_SEARCHING) {
    mode = MODE_SEARCHING;
  } else {
    mode = MODE_READY;
  }

  /* State changes render immediately; animations are non-blocking. */
  if ((uint8_t)mode == sLastMode &&
      (uint32_t)(now - sLastFrame) < SHOWDUINO_ESTOP_STATUS_PIXEL_FRAME_MS) {
    return;
  }
  sLastMode = (uint8_t)mode;
  sLastFrame = now;
  sPixels.clear();

  const uint8_t low = SHOWDUINO_ESTOP_STATUS_PIXEL_BRIGHTNESS;
  const bool flip = ((now / 320UL) & 1U) != 0U;

  switch (mode) {
    case MODE_EMERGENCY:
      sModeName = "EMERGENCY";
      /* Fixed white, irrespective of search / local animation / Locate. */
      rgb(0, 255, 255, 255, SHOWDUINO_ESTOP_STATUS_PIXEL_EMERGENCY_BRIGHTNESS);
      rgb(1, 255, 255, 255, SHOWDUINO_ESTOP_STATUS_PIXEL_EMERGENCY_BRIGHTNESS);
      break;
    case MODE_FAULT:
      sModeName = "FAULT";
      /* Alternating magenta and amber. */
      rgb(0, flip ? 255 : 255, flip ? 0 : 100, flip ? 130 : 0, low);
      rgb(1, flip ? 255 : 255, flip ? 100 : 0, flip ? 0 : 130, low);
      break;
    case MODE_LOCATE:
      sModeName = "LOCATE";
      rgb(flip ? 1 : 0, 255, 255, 255, low);
      break;
    case MODE_SEARCHING:
      sModeName = "SEARCHING";
      /* Alternating amber: station alive, radio not yet fresh. */
      rgb(flip ? 1 : 0, 255, 80, 0, low);
      break;
    case MODE_BOOT:
      sModeName = "BOOT";
      rgb(0, 100, 0, 180, low);
      rgb(1, 35, 0, 60, low);
      break;
    case MODE_READY:
    default: {
      sModeName = "READY";
      /* Pixel 0 steady turquoise; pixel 1 slow violet breathing. */
      rgb(0, 0, 175, 130, low);
      const uint16_t phase = (uint16_t)((now / 8UL) % 400UL);
      const uint16_t wave = phase < 200U ? phase : (400U - phase);
      const uint8_t breathe = (uint8_t)(4U + (wave * (uint16_t)(low - 4U)) / 200U);
      rgb(1, 90, 12, 200, breathe);
      break;
    }
  }
  sPixels.show();
}

bool emergencyStatusPixelsReady() { return sReady; }
const char *emergencyStatusPixelsMode() { return sModeName; }
