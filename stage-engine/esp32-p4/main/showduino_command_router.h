#ifndef SHOWDUINO_COMMAND_ROUTER_H
#define SHOWDUINO_COMMAND_ROUTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*showduino_response_callback_t)(const char *response, void *context);

void showduino_command_router_init(showduino_response_callback_t callback, void *context);
void showduino_command_router_handle(const char *command);

#ifdef __cplusplus
}
#endif

#endif
