#include "showduino_runtime.h"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static showduino_runtime_status_t s_status;
static SemaphoreHandle_t s_lock;

void showduino_runtime_init(void) {
    memset(&s_status, 0, sizeof(s_status));
    s_lock = xSemaphoreCreateMutex();
    s_status.state = SHOWDUINO_STATE_BOOTING;
    s_status.boot_time_us = esp_timer_get_time();
}

void showduino_runtime_set_state(showduino_runtime_state_t state) {
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
}

showduino_runtime_state_t showduino_runtime_get_state(void) {
    showduino_runtime_state_t state;
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    state = s_status.state;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
    return state;
}

void showduino_runtime_latch_emergency(void) {
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.emergency_latched = true;
    s_status.state = SHOWDUINO_STATE_EMERGENCY;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
}

void showduino_runtime_clear_emergency(void) {
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.emergency_latched = false;
    s_status.state = SHOWDUINO_STATE_READY;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
}

bool showduino_runtime_is_emergency_latched(void) {
    bool latched;
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    latched = s_status.emergency_latched;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
    return latched;
}

void showduino_runtime_record_command(const char *command, bool accepted) {
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    if (accepted) s_status.commands_received++;
    else s_status.commands_rejected++;
    s_status.last_command_time_us = esp_timer_get_time();
    if (command != NULL) {
        strncpy(s_status.last_command, command, sizeof(s_status.last_command) - 1);
        s_status.last_command[sizeof(s_status.last_command) - 1] = '\0';
    }
    if (s_lock != NULL) xSemaphoreGive(s_lock);
}

showduino_runtime_status_t showduino_runtime_get_status(void) {
    showduino_runtime_status_t copy;
    if (s_lock != NULL) xSemaphoreTake(s_lock, portMAX_DELAY);
    copy = s_status;
    if (s_lock != NULL) xSemaphoreGive(s_lock);
    return copy;
}

const char *showduino_runtime_state_name(showduino_runtime_state_t state) {
    switch (state) {
        case SHOWDUINO_STATE_BOOTING: return "BOOTING";
        case SHOWDUINO_STATE_READY: return "READY";
        case SHOWDUINO_STATE_RUNNING: return "RUNNING";
        case SHOWDUINO_STATE_STOPPED: return "STOPPED";
        case SHOWDUINO_STATE_EMERGENCY: return "EMERGENCY";
        case SHOWDUINO_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
