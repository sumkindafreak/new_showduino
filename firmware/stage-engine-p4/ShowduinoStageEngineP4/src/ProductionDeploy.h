#ifndef SHOWDUINO_PRODUCTION_DEPLOY_H
#define SHOWDUINO_PRODUCTION_DEPLOY_H

#include <Arduino.h>
#include <stddef.h>

/*
 * SHDO v2 persist session. Studio/WebUI posts begin/chunk/commit over
 * WEB/BODY. The P4 compiles, writes format-v1 SD files, and does not
 * auto-load or auto-start. Emergency aborts commit.
 */
bool productionDeployTry(const char *method, const char *path,
                         const char *body, size_t len,
                         int *statusOut, String *jsonOut);

#endif
