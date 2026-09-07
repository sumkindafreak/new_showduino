#include "AudioProtocol.h"

void audioProtocolFormatAccepted(char *out, size_t n, uint32_t id) {
  if (!out || !n) return;
  snprintf(out, n, "AUDIO:ACCEPTED:%lu", (unsigned long)id);
}

void audioProtocolFormatRejected(char *out, size_t n, uint32_t id, ShowduinoAudioFail fail) {
  if (!out || !n) return;
  snprintf(out, n, "AUDIO:FAILED:%lu:%s", (unsigned long)id, showduino_audio_fail_name(fail));
}

void audioProtocolFormatStarted(char *out, size_t n, uint32_t id, const char *rel) {
  if (!out || !n) return;
  snprintf(out, n, "AUDIO:STARTED:%lu:%s", (unsigned long)id, rel ? rel : "");
}

void audioProtocolFormatCompleted(char *out, size_t n, uint32_t id, const char *rel) {
  if (!out || !n) return;
  snprintf(out, n, "AUDIO:COMPLETED:%lu:%s", (unsigned long)id, rel ? rel : "");
}

void audioProtocolFormatFailed(char *out, size_t n, uint32_t id, ShowduinoAudioFail fail) {
  if (!out || !n) return;
  snprintf(out, n, "AUDIO:FAILED:%lu:%s", (unsigned long)id, showduino_audio_fail_name(fail));
}

void audioProtocolFormatAnnounce(char *out, size_t n, const char *mac, const char *fw,
                                 const char *state) {
  if (!out || !n) return;
  snprintf(out, n, "ANNOUNCE:%s:%s:%s", mac ? mac : "00:00:00:00:00:00",
           fw ? fw : "0.0.0", state ? state : "UNKNOWN");
}

void audioProtocolFormatStatus(char *out, size_t n, const char *state, uint8_t volume,
                               const char *rel, const char *fault) {
  if (!out || !n) return;
  snprintf(out, n, "STATUS:%s:V%u:%s:%s", state ? state : "UNKNOWN",
           (unsigned)volume, rel && rel[0] ? rel : "-",
           fault && fault[0] ? fault : "-");
}
