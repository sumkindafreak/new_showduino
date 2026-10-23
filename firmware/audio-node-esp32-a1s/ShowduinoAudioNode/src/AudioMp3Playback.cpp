#include "AudioMp3Playback.h"
#include "../BoardConfig.h"

#include <Arduino.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include <ESP_I2S.h>
#include <new>

/* ESP8266Audio 2.4.1 performs MP3 decode; Showduino keeps exclusive
 * ownership of its A1S I2S pins and the ES8388 codec register setup.
 * This implementation never requests PSRAM. */
static AudioFileSourceSD *sSource = nullptr;
static AudioGeneratorMP3 *sDecoder = nullptr;
static char sLastError[48] = "";
static uint32_t sFileSize = 0;

static void mp3Error(const char *msg) {
  snprintf(sLastError, sizeof(sLastError), "%s", msg ? msg : "");
}

class ShowduinoMp3Output final : public AudioOutput {
 public:
  bool begin() override {
    sUsed = 0;
    sRate = 44100;
    sChannels = 2;
    sFailed = false;
    return true;
  }

  bool SetRate(int hz) override {
    if (hz != 8000 && hz != 11025 && hz != 12000 &&
        hz != 16000 && hz != 22050 && hz != 24000 &&
        hz != 32000 && hz != 44100 && hz != 48000) {
      mp3Error("Unsupported MP3 sample rate");
      sFailed = true;
      return false;
    }
    if (sI2sOn && sRate != hz) {
      if (!flushPcm()) return false;
      sI2s.end();
      sI2sOn = false;
    }
    sRate = hz;
    return AudioOutput::SetRate(hz);
  }

  bool SetChannels(int channels) override {
    if (channels != 1 && channels != 2) {
      mp3Error("Unsupported MP3 channels");
      sFailed = true;
      return false;
    }
    sChannels = channels;
    return AudioOutput::SetChannels(channels);
  }

  bool ConsumeSample(int16_t samples[2]) override {
    if (sFailed) return false;
    if (sUsed >= kSamples && !flushPcm()) return false;
    sPcm[sUsed++] = samples[0];
    sPcm[sUsed++] = sChannels == 1 ? samples[0] : samples[1];
    if (sUsed >= kSamples) return flushPcm();
    return true;
  }

  uint16_t ConsumeSamples(int16_t *samples, uint16_t count) override {
    uint16_t consumed = 0;
    while (consumed < count && ConsumeSample(samples + consumed * 2u)) {
      ++consumed;
    }
    return consumed;
  }

  void flush() override { flushPcm(); }

  bool stop() override {
    const bool ok = flushPcm();
    if (sI2sOn) {
      sI2s.end();
      sI2sOn = false;
    }
    sUsed = 0;
    return ok;
  }

  bool failed() const { return sFailed; }
  void reset() {
    if (sI2sOn) {
      sI2s.end();
      sI2sOn = false;
    }
    sUsed = 0;
    sRate = 44100;
    sChannels = 2;
    sFailed = false;
  }

 private:
  /* 4096 bytes = 1024 stereo frames, giving larger DMA writes and much less
   * SPI/I2S overhead than forwarding individual decoded samples. */
  static constexpr size_t kSamples = 2048;
  int16_t sPcm[kSamples] = {};
  size_t sUsed = 0;
  uint32_t sRate = 44100;
  uint8_t sChannels = 2;
  bool sI2sOn = false;
  bool sFailed = false;
  I2SClass sI2s;

  bool flushPcm() {
    if (sFailed) return false;
    if (sUsed == 0) return true;

    if (!sI2sOn) {
      sI2s.setPins(SHOWDUINO_AUDIO_I2S_BCLK, SHOWDUINO_AUDIO_I2S_WS,
                   SHOWDUINO_AUDIO_I2S_DOUT, SHOWDUINO_AUDIO_I2S_DIN,
                   SHOWDUINO_AUDIO_I2S_MCLK);
      if (!sI2s.begin(I2S_MODE_STD, sRate, I2S_DATA_BIT_WIDTH_16BIT,
                      I2S_SLOT_MODE_STEREO)) {
        mp3Error("MP3 I2S start failed");
        sFailed = true;
        return false;
      }
      sI2sOn = true;
    }

    const uint8_t *pcm = reinterpret_cast<const uint8_t *>(sPcm);
    const size_t total = sUsed * sizeof(int16_t);
    size_t offset = 0;
    while (offset < total) {
      const size_t n = sI2s.write(pcm + offset, total - offset);
      if (n == 0 || (n & 1u) != 0 || n > total - offset) {
        mp3Error("MP3 I2S write failed");
        sFailed = true;
        return false;
      }
      offset += n;
    }
    sUsed = 0;
    return true;
  }
};

static ShowduinoMp3Output sOutput;

void audioMp3Stop() {
  if (sDecoder) {
    sDecoder->stop();
    delete sDecoder;
    sDecoder = nullptr;
  }
  sOutput.stop();
  sOutput.reset();
  if (sSource) {
    sSource->close();
    delete sSource;
    sSource = nullptr;
  }
}

bool audioMp3Start(const char *absPath) {
  audioMp3Stop();
  mp3Error("");
  sFileSize = 0;
  if (!absPath || !absPath[0]) {
    mp3Error("Empty MP3 path");
    return false;
  }

  sSource = new (std::nothrow) AudioFileSourceSD(absPath);
  if (!sSource || !sSource->isOpen() || !sSource->getSize()) {
    mp3Error("MP3 SD open failed");
    audioMp3Stop();
    return false;
  }
  sFileSize = sSource->getSize();

  sDecoder = new (std::nothrow) AudioGeneratorMP3();
  if (!sDecoder) {
    mp3Error("No heap for MP3 decoder");
    audioMp3Stop();
    return false;
  }
  sOutput.reset();
  if (!sDecoder->begin(sSource, &sOutput)) {
    mp3Error("MP3 decoder start failed");
    audioMp3Stop();
    return false;
  }
  Serial.printf("[AUDIO] MP3 decoder ready, file=%lu bytes heap=%lu\\n",
                (unsigned long)sFileSize, (unsigned long)ESP.getFreeHeap());
  return true;
}

bool audioMp3Service() {
  if (!sDecoder || sOutput.failed()) return false;
  if (!sDecoder->isRunning()) return false;
  const bool alive = sDecoder->loop();
  return alive && !sOutput.failed() && sDecoder->isRunning();
}

uint32_t audioMp3Position() {
  return sSource ? sSource->getPos() : sFileSize;
}

uint32_t audioMp3Size() { return sFileSize; }
const char *audioMp3LastError() { return sLastError; }
