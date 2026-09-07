#ifndef SHOWDUINO_AUDIO_PROTOCOL_H
#define SHOWDUINO_AUDIO_PROTOCOL_H

#include <Arduino.h>
#include "../../../protocol/showduino_audio_node.h"

void audioProtocolFormatAccepted(char *out, size_t n, uint32_t id);
void audioProtocolFormatRejected(char *out, size_t n, uint32_t id, ShowduinoAudioFail fail);
void audioProtocolFormatStarted(char *out, size_t n, uint32_t id, const char *rel);
void audioProtocolFormatCompleted(char *out, size_t n, uint32_t id, const char *rel);
void audioProtocolFormatFailed(char *out, size_t n, uint32_t id, ShowduinoAudioFail fail);
void audioProtocolFormatAnnounce(char *out, size_t n, const char *mac, const char *fw,
                                 const char *state);
void audioProtocolFormatStatus(char *out, size_t n, const char *state, uint8_t volume,
                               const char *rel, const char *fault);

#endif
