#ifndef SHOWDUINO_PLUGIN_JSON_H
#define SHOWDUINO_PLUGIN_JSON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool pluginJsonExtractString(const char *json, const char *key, char *out, size_t outLen);
bool pluginJsonExtractU32(const char *json, const char *key, uint32_t *out);
bool pluginJsonExtractHexByte(const char *json, const char *key, uint8_t *out);
bool pluginJsonObjectSlice(const char *json, const char *key, const char **begin, const char **end);
int pluginJsonArrayObjectCount(const char *json, const char *arrayKey);
bool pluginJsonArrayObjectAt(const char *json, const char *arrayKey, int index,
                             const char **begin, const char **end);
bool pluginJsonParseAddressRange(const char *text, uint8_t *minAddr, uint8_t *maxAddr);
uint32_t pluginJsonParseCapabilities(const char *json);

#endif
