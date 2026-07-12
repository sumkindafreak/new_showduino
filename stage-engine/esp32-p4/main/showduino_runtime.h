#ifndef SHOWDUINO_RUNTIME_H
#define SHOWDUINO_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHOWDUINO_STATE_BOOTING = 0,
    SHOWDUINO_STATE_READY,
    SHOWDUINO_STATE_RUNNING,
    SHOWDUINO_STATE_STOPPED,
    SHOWDUINO_STATE_EMERGENCY,
    SHOWDUINO_STATE_ERROR
} showduino_runtime_state_t;

typedef struct {
    showduino_runtime_state_t state;
    bool emergency_latched;
    uint32_t commands_received;
    uint32_t commands_rejected;
    int64_t boot_time_us;
    int64_t last_command_time_us;
    char last_command[96];
} showduino_runtime_status_t;

void showduino_runtime_init(void);
void showduino_runtime_set_state(showduino_runtime_state_t state);
showduino_runtime_state_t showduino_runtime_get_state(void);
void showduino_runtime_latch_emergency(void);
void showduino_runtime_clear_emergency(void);
bool showduino_runtime_is_emergency_latched(void);
void showduino_runtime_record_command(const char *command, bool accepted);
showduino_runtime_status_t showduino_runtime_get_status(void);
const char *showduino_runtime_state_name(showduino_runtime_state_t state);

#ifdef __cplusplus
}
#endif

#endif
