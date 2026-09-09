#ifndef SHOWDUINO_NODE_CONFIG_H
#define SHOWDUINO_NODE_CONFIG_H

#include <Arduino.h>

bool nodeConfigBegin(const char *ns);
void nodeConfigGetName(char *out, size_t n, const char *fallback);
void nodeConfigSetName(const char *name);
uint8_t nodeConfigGetU8(const char *key, uint8_t fallback);
void nodeConfigSetU8(const char *key, uint8_t value);

#endif
