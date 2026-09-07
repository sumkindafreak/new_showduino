#include "AudioInput.h"
#include "AudioInputConfig.h"
#include "AudioLevelMeter.h"
#include "SoundTriggerEngine.h"
#include "../AudioCodec.h"
#include "../AudioPlayback.h"
#include "../AudioStorage.h"
#include "../AudioNodeState.h"
#include "../AudioCommand.h"
#include "../EspNowNodeTransport.h"
#include "../../BoardConfig.h"
#include "../../../protocol/showduino_protocol_version.h"

#include <ESP_I2S.h>
#include <SD.h>

static I2SClass sCap;
static bool sCapOn = false;
static bool sWantCap = true;
static bool sReady = false;
static bool sFault = false;
static bool sEmergency = false;
static bool sMonitor = false;
static bool sRecording = false;
static bool sLocalPlayReq = false;
static uint32_t sOverruns = 0;
static uint32_t sLastFeed = 0;
static uint32_t sLastStatusTx = 0;
static uint32_t sLastMon = 0;
static uint32_t sLastCalProg = 0;
static uint32_t sRecStart = 0;
static uint32_t sRecBytes = 0;
static File sRec;
static char sErr[40] = "";
static int16_t sBuf[256];

static void setErr(const char *m) {
  strncpy(sErr, m ? m : "", sizeof(sErr) - 1);
}

static const char *readyTok() {
  if (sFault || !sReady) return "FLT";
  if (soundTriggerEngine()->calibrating) return "CAL";
  if (!audioInputConfig().enabled) return "OFF";
  return "RDY";
}

static void stopCapture() {
  if (!sCapOn) return;
  sCap.end();
  sCapOn = false;
}

static bool startCapture() {
  if (sCapOn) return true;
  if (audioPlaybackActive() || audioPlaybackPaused()) return false;
  sCap.setPins(SHOWDUINO_AUDIO_I2S_BCLK, SHOWDUINO_AUDIO_I2S_WS,
               SHOWDUINO_AUDIO_I2S_DOUT, SHOWDUINO_AUDIO_I2S_DIN,
               SHOWDUINO_AUDIO_I2S_MCLK);
  if (!sCap.begin(I2S_MODE_STD, SHOWDUINO_SOUND_RATE_HZ,
                  I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    setErr("I2S capture failed");
    sFault = true;
    soundTriggerSetInputReady(false, true);
    return false;
  }
  sCapOn = true;
  sFault = false;
  setErr("");
  soundTriggerSetInputReady(true, false);
  return true;
}

static void finishRecord(bool ok) {
  if (!sRecording) return;
  if (sRec) {
    if (ok && sRecBytes > 0) {
      uint32_t dataSz = sRecBytes;
      uint32_t riffSz = 36 + dataSz;
      sRec.seek(4);
      sRec.write((uint8_t *)&riffSz, 4);
      sRec.seek(40);
      sRec.write((uint8_t *)&dataSz, 4);
    }
    sRec.close();
  }
  sRecording = false;
  Serial.printf("[SOUND] RECORD %s bytes=%lu\n", ok ? "done" : "stop",
                (unsigned long)sRecBytes);
}

static void writeWavHeader(File &f) {
  const uint32_t rate = SHOWDUINO_SOUND_RATE_HZ;
  const uint16_t ch = 2;
  const uint16_t bits = 16;
  const uint32_t bps = rate * ch * (bits / 8);
  const uint16_t align = ch * (bits / 8);
  uint8_t h[44];
  memset(h, 0, sizeof(h));
  memcpy(h, "RIFF", 4);
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16;
  h[20] = 1;
  h[22] = (uint8_t)ch;
  h[24] = (uint8_t)(rate & 0xFF);
  h[25] = (uint8_t)((rate >> 8) & 0xFF);
  h[26] = (uint8_t)((rate >> 16) & 0xFF);
  h[27] = (uint8_t)((rate >> 24) & 0xFF);
  h[28] = (uint8_t)(bps & 0xFF);
  h[29] = (uint8_t)((bps >> 8) & 0xFF);
  h[30] = (uint8_t)((bps >> 16) & 0xFF);
  h[31] = (uint8_t)((bps >> 24) & 0xFF);
  h[32] = (uint8_t)align;
  h[34] = (uint8_t)bits;
  memcpy(h + 36, "data", 4);
  f.write(h, 44);
}

static void reportLine(const char *line) {
  if (!line || !line[0]) return;
  Serial.print("[SOUND] ");
  Serial.println(line);
  audioEspNowSend(line, 0);
}

bool audioInputBegin() {
  sWantCap = audioInputConfig().enabled != 0;
  sReady = false;
  sFault = false;
  audioLevelMeterReset();
  soundTriggerBegin(&audioInputConfig());
  uint8_t floor = 0;
  if (audioInputConfigLoadCal(&floor)) {
    soundTriggerEngine()->noiseFloor = floor;
    soundTriggerEngine()->calibrated = 1;
  }
  if (!audioCodecEnableInput()) {
    setErr(audioCodecLastError()[0] ? audioCodecLastError() : "ADC init failed");
    sFault = true;
    soundTriggerSetInputReady(false, true);
    Serial.printf("[SOUND] ADC init failed: %s\n", sErr);
    return false;
  }
  sReady = true;
  soundTriggerSetInputReady(true, false);
  if (sWantCap) startCapture();
  sLastFeed = millis();
  Serial.printf("[SOUND] input ready rate=%u duplex=PLAYBACK_ONLY\n",
                (unsigned)SHOWDUINO_SOUND_RATE_HZ);
  return true;
}

void audioInputOnPlaybackStart() {
  sWantCap = false;
  if (sRecording) finishRecord(false);
  stopCapture();
}

void audioInputOnPlaybackStop() {
  soundTriggerNotePlaybackStop(millis());
  sWantCap = audioInputConfig().enabled != 0 &&
             audioInputConfig().duplex != SHOWDUINO_SOUND_DUPLEX_PLAYBACK_ONLY
                 ? true
                 : (audioInputConfig().enabled != 0);
  /* Conservative default: resume capture only after playback I2S is released. */
  sWantCap = audioInputConfig().enabled != 0;
}

void audioInputSetEmergency(bool on) {
  sEmergency = on;
  if (on) {
    ShowduinoSoundEngine *e = soundTriggerEngine();
    e->pending.type = SHOWDUINO_SOUND_EVT_NONE;
    e->armed = 0;
  }
}

void audioInputService() {
  const uint32_t now = millis();
  const bool playing = audioPlaybackActive() || audioPlaybackPaused();
  const bool commsOk = audioEspNowHaveComms();
  const bool emergency = sEmergency || audioNodeState() == SHOWDUINO_AUDIO_ST_EMERGENCY;

  if (sWantCap && !playing && !sCapOn) startCapture();
  if ((!sWantCap || playing) && sCapOn) stopCapture();

  if (sCapOn) {
    const int n = (int)sCap.readBytes((char *)sBuf, sizeof(sBuf));
    if (n <= 0) {
      sOverruns++;
    } else {
      const size_t frames = (size_t)n / 4;
      audioLevelMeterAddFrames(sBuf, frames);
      if (sRecording && sRec) {
        const size_t w = sRec.write((uint8_t *)sBuf, (size_t)n);
        sRecBytes += (uint32_t)w;
        if (w != (size_t)n) sOverruns++;
        if ((now - sRecStart) >= SHOWDUINO_SOUND_RECORD_MAX_MS) finishRecord(true);
      }
    }
  }

  if ((int32_t)(now - sLastFeed) >= 20) {
    const uint32_t dt = now - sLastFeed;
    sLastFeed = now;
    audioLevelMeterEndBlock();
    soundTriggerFeed(audioLevelMeterRms(), audioLevelMeterPeak(), now, dt,
                     playing ? 1 : 0, emergency ? 1 : 0, commsOk ? 1 : 0);

    ShowduinoSoundEngine *e = soundTriggerEngine();
    if (e->calibrating && (int32_t)(now - sLastCalProg) >= 400) {
      sLastCalProg = now;
      const uint32_t elapsed = now - e->calStartMs;
      uint32_t pct = (elapsed * 100u) / (e->cfg.calibrateMs ? e->cfg.calibrateMs : 4000u);
      if (pct > 99) pct = 99;
      char prog[40];
      snprintf(prog, sizeof(prog), "SOUND:CALIBRATE:PROGRESS:%lu", (unsigned long)pct);
      reportLine(prog);
    }
    if (!e->calibrating && e->calibrated && e->calStartMs &&
        (int32_t)(now - e->calStartMs) >= (int32_t)e->cfg.calibrateMs &&
        (int32_t)(now - sLastCalProg) < 2000) {
      audioInputConfigSaveCal(e->noiseFloor);
      char done[40];
      snprintf(done, sizeof(done), "SOUND:CALIBRATE:DONE,F=%u", (unsigned)e->noiseFloor);
      reportLine(done);
      e->calStartMs = 0;
    }

    ShowduinoSoundEvent ev;
    if (soundTriggerTakeEvent(&ev)) {
      char line[SHOWDUINO_NODE_COMMAND_MAX];
      showduino_sound_format_trigger(line, sizeof(line), &ev);
      if (commsOk && !emergency) reportLine(line);
      else Serial.printf("[SOUND] dropped (comms/emergency) %s\n", line);
      if (soundTriggerLocalTest() && !audioNodeStateShowControlled() && !emergency) {
        sLocalPlayReq = true;
      }
    }
  }

  if (sLocalPlayReq) {
    sLocalPlayReq = false;
    Serial.println("[SOUND] LOCAL_TEST_TRIGGER play system-test.wav");
    audioCommandApply("AUDIO:NODE:PLAY:" SHOWDUINO_AUDIO_TEST_FILE, 0, false);
  }

  if (sMonitor && (int32_t)(now - sLastMon) >= 100) {
    sLastMon = now;
    Serial.printf("[SOUND] L=%u P=%u F=%u T=%u A=%u C=%lu\n",
                  (unsigned)audioInputLevel(), (unsigned)audioInputPeak(),
                  (unsigned)audioInputNoiseFloor(),
                  (unsigned)showduino_sound_fire_level(soundTriggerEngine()),
                  (unsigned)soundTriggerEngine()->armed,
                  (unsigned long)showduino_sound_cooldown_remaining(soundTriggerEngine(), now));
  }

  if (audioInputConfig().enabled && sReady &&
      (int32_t)(now - sLastStatusTx) >= 500) {
    sLastStatusTx = now;
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    audioInputFormatStatus(line, sizeof(line));
    audioEspNowSend(line, 0);
  }
}

bool audioInputReady() { return sReady && !sFault; }
bool audioInputFault() { return sFault; }
uint8_t audioInputLevel() { return soundTriggerEngine()->smoothed; }
uint8_t audioInputPeak() { return soundTriggerEngine()->instPeak; }
uint8_t audioInputNoiseFloor() { return soundTriggerEngine()->noiseFloor; }
const char *audioInputLastError() { return sErr; }

void audioInputFormatStatus(char *out, size_t n) {
  const ShowduinoSoundEngine *e = soundTriggerEngine();
  showduino_sound_format_status(out, n, readyTok(),
                                e->smoothed, e->instPeak, e->noiseFloor,
                                showduino_sound_fire_level(e),
                                e->armed,
                                showduino_sound_cooldown_remaining(e, millis()),
                                showduino_sound_event_wire(e->lastEventType),
                                e->calibrated);
}

int audioInputTakeEventLine(char *out, size_t n) {
  ShowduinoSoundEvent ev;
  if (!soundTriggerTakeEvent(&ev)) return 0;
  return showduino_sound_format_trigger(out, n, &ev);
}

void audioInputCalibrate() {
  soundTriggerStartCalibrate(millis());
  soundTriggerEngine()->calStartMs = millis();
  sLastCalProg = 0;
  reportLine("SOUND:CALIBRATE:PROGRESS:0");
}

void audioInputEnable(bool on) {
  audioInputConfigSetEnabled(on, true);
  soundTriggerSetEnabled(on);
  sWantCap = on;
  if (!on && sCapOn && !audioPlaybackActive() && !audioPlaybackPaused()) {
    /* Keep capture for diagnostics meter even when triggers disabled. */
  }
  Serial.printf("[SOUND] %s\n", on ? "ENABLE" : "DISABLE");
}

void audioInputSetThreshold(uint8_t threshold) {
  audioInputConfigSetThreshold(threshold, true);
  soundTriggerApplyConfig(&audioInputConfig());
}

void audioInputTriggerTest() {
  soundTriggerEmitTest(millis());
}

void audioInputSetMonitor(bool on) { sMonitor = on; }
bool audioInputMonitor() { return sMonitor; }

void audioInputPrintStatus() {
  const ShowduinoSoundEngine *e = soundTriggerEngine();
  Serial.println("[SOUND]");
  Serial.printf("Input: %s\n", readyTok());
  Serial.printf("Level: %u\n", (unsigned)e->smoothed);
  Serial.printf("Peak: %u\n", (unsigned)e->instPeak);
  Serial.printf("Noise Floor: %u\n", (unsigned)e->noiseFloor);
  Serial.printf("Threshold: %u\n", (unsigned)showduino_sound_fire_level(e));
  Serial.printf("Armed: %s\n", e->armed ? "YES" : "NO");
  Serial.printf("Cooldown: %lu\n",
                (unsigned long)showduino_sound_cooldown_remaining(e, millis()));
  Serial.printf("Last Trigger: %s\n", showduino_sound_event_name(e->lastEventType));
  Serial.printf("Overruns: %lu\n", (unsigned long)sOverruns);
}

bool audioInputStartRecordTest() {
  if (sRecording) return false;
  if (!audioStorageReady() || !audioStorageWritable()) {
    Serial.println("[SOUND] RECORD unavailable - no SD");
    return false;
  }
  if (!SD.exists(PATH_AUDIO_RECORDINGS)) SD.mkdir(PATH_AUDIO_RECORDINGS);
  if (audioPlaybackActive() || audioPlaybackPaused()) {
    Serial.println("[SOUND] RECORD rejected - playback owns I2S");
    return false;
  }
  sWantCap = true;
  if (!startCapture()) return false;
  sRec = SD.open("/showduino/recordings/diag.wav", FILE_WRITE);
  if (!sRec) {
    Serial.println("[SOUND] RECORD open failed");
    return false;
  }
  writeWavHeader(sRec);
  sRecStart = millis();
  sRecBytes = 0;
  sRecording = true;
  Serial.println("[SOUND] RECORD start max 8s");
  return true;
}

bool audioInputRecording() { return sRecording; }

void audioInputSetLocalTest(bool on) {
  soundTriggerSetLocalTest(on);
  Serial.printf("[SOUND] LOCAL_TEST_TRIGGER %s\n", on ? "ON" : "OFF");
}
