#ifndef SHOWDUINO_AUDIO_STORAGE_H
#define SHOWDUINO_AUDIO_STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include "../../../protocol/showduino_audio_node.h"

bool audioStorageBegin();
bool audioStorageReady();
bool audioStorageWritable();
bool audioStorageConfigFault();
fs::FS &audioStorageFs();
const ShowduinoAudioConfig &audioStorageConfig();
void audioStorageSetVolume(uint8_t percent);
void audioStorageLoop();
bool audioStorageResolve(const char *rel, char *out, size_t outLen);
bool audioStorageExists(const char *absPath);
uint16_t audioStorageListWav(char names[][40], uint16_t maxNames);
uint16_t audioStorageInventory(char names[][40], uint16_t maxNames);
bool audioStorageShowLibraryRescan();
uint16_t audioStorageShowLibraryCount();
bool audioStorageShowLibraryPage(uint16_t page, char *path, size_t pathLen);
const char *audioStorageLastError();
const char *audioStorageStateName();
bool audioStorageCardPresent();
bool audioStorageJustRemoved();
bool audioStorageJustInserted();
bool audioStorageWriteConfig();

#endif
