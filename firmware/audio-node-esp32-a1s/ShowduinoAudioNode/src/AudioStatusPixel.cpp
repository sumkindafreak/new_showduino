#include <Adafruit_NeoPixel.h>
#include "../BoardConfig.h"
#include "AudioStatusPixel.h"
#include "AudioPixelOutput.h"
#include "AudioNodeState.h"
#include "AudioPixelEngine.h"
#include "input/AudioInput.h"

static Adafruit_NeoPixel sPixel(SHOWDUINO_AUDIO_STATUS_PIXEL_COUNT,
    SHOWDUINO_AUDIO_STATUS_PIXEL_PIN, SHOWDUINO_AUDIO_STATUS_PIXEL_ORDER);
static AudioPixelOutput sStatusOutput;
static bool sOutputReady = false;
static uint32_t sLastWriteMs = 0;
static bool sTesting = false;
static uint32_t sTestStart = 0;
static uint32_t sColour = 0xFFFFFFFF;

void audioStatusPixelTest() {
  sTesting = true; sTestStart = millis();
  Serial.printf("[STATUS PIXEL] GPIO%d RGB test requested, output=%s\n",
                SHOWDUINO_AUDIO_STATUS_PIXEL_PIN, sOutputReady ? "READY" : "FAILED");
}

void audioStatusPixelService() {
  const uint32_t now = millis();
  const auto state = audioNodeState();
  const auto owner = audioOwnerMode();
  const uint8_t phase = (now / 8U) % 128U;
  const uint8_t pulse = 8U + (phase < 64U ? phase : 127U - phase);
  uint32_t colour;
  if (state == SHOWDUINO_AUDIO_ST_EMERGENCY ||
      owner == SHOWDUINO_OWNER_EMERGENCY || audioPixelEngineEmergency()) {
    sTesting = false;
    colour = sPixel.Color(255, 255, 255);
  } else if (state == SHOWDUINO_AUDIO_ST_FAULT ||
             state == SHOWDUINO_AUDIO_ST_NO_STORAGE ||
             audioNodeStateFault() != SHOWDUINO_AUDIO_FAIL_NONE) {
    sTesting = false;
    colour = (now / 500U) % 2U ? sPixel.Color(80, 0, 80) : sPixel.Color(80, 35, 0);
  } else if (audioPixelEngineLocateActive()) {
    colour = (now / 150U) % 2U ? sPixel.Color(160, 160, 160) : 0;
  } else if (sTesting && (now - sTestStart) < 1500U) {
    const uint32_t stage = (now - sTestStart) / 500U;
    colour = sPixel.Color(stage == 0 ? 32 : 0, stage == 1 ? 32 : 0, stage == 2 ? 32 : 0);
  } else {
    sTesting = false;
    if (audioInputRecording()) colour = sPixel.Color(pulse, 0, pulse);
    else if (state == SHOWDUINO_AUDIO_ST_PLAYING || state == SHOWDUINO_AUDIO_ST_LOOPING)
      colour = sPixel.Color(0, 40, 0);
    else if (state == SHOWDUINO_AUDIO_ST_PAUSED) colour = sPixel.Color(40, 18, 0);
    else if (state == SHOWDUINO_AUDIO_ST_BOOTING) colour = sPixel.Color(pulse / 2, 0, pulse);
    else if (owner == SHOWDUINO_OWNER_SEARCHING) colour = sPixel.Color(pulse, pulse / 3, 0);
    else if (audioNodeStateAuthorityFresh(SHOWDUINO_OWNER_KEEPALIVE_MS)) colour = sPixel.Color(0, 24, 16);
    else if (audioNodeStateLastCommsMs() != 0 &&
             (now - audioNodeStateLastCommsMs()) < SHOWDUINO_AUDIO_COMMS_TIMEOUT_MS)
      colour = sPixel.Color(0, 16, 24);
    else colour = sPixel.Color(12, 0, 24);
  }
  if (sOutputReady && (colour != sColour || now - sLastWriteMs >= 250U)) {
    sPixel.setPixelColor(0, colour);
    if (sStatusOutput.write(sPixel.getPixels(), SHOWDUINO_AUDIO_STATUS_PIXEL_COUNT * 3U)) {
      sColour = colour;
      sLastWriteMs = now;
    }
  }
}

void audioStatusPixelBegin() {
  sPixel.begin();
  sOutputReady = sStatusOutput.begin(SHOWDUINO_AUDIO_STATUS_PIXEL_PIN, SHOWDUINO_AUDIO_STATUS_PIXEL_COUNT * 3U);
  audioStatusPixelService();
}
