#ifndef SHOWDUINO_AUDIO_MP3_PLAYBACK_H
#define SHOWDUINO_AUDIO_MP3_PLAYBACK_H

#include <Arduino.h>

/* MP3 decoder is deliberately isolated from WAV playback and Showduino
 * command/ownership code. All calls must hold AudioPlayback's mutex. */
bool audioMp3Start(const char *absPath);
bool audioMp3Service();
void audioMp3Stop();
uint32_t audioMp3Position();
uint32_t audioMp3Size();
const char *audioMp3LastError();

#endif
