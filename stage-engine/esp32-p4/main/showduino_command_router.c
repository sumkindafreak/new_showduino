#include "showduino_command_router.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "showduino_gpio_service.h"
#include "showduino_runtime.h"

static const char *TAG = "showduino_router";
static showduino_response_callback_t s_response_callback;
static void *s_response_context;

static void respond(const char *message) {
    ESP_LOGI(TAG, "Response: %s", message);
    if (s_response_callback != NULL) {
        s_response_callback(message, s_response_context);
    }
}

static void respond_status(void) {
    const showduino_runtime_status_t status = showduino_runtime_get_status();
    char response[192];

    snprintf(
        response,
        sizeof(response),
        "STATUS:STATE=%s;EMERGENCY=%u;GPIO23=%u;RX=%lu;REJECTED=%lu",
        showduino_runtime_state_name(status.state),
        status.emergency_latched ? 1U : 0U,
        showduino_gpio_service_get_test_output() ? 1U : 0U,
        (unsigned long)status.commands_received,
        (unsigned long)status.commands_rejected
    );

    respond(response);
}

void showduino_command_router_init(showduino_response_callback_t callback, void *context) {
    s_response_callback = callback;
    s_response_context = context;
}

void showduino_command_router_handle(const char *command) {
    if (command == NULL || command[0] == '\0') {
        showduino_runtime_record_command("EMPTY", false);
        respond("ERROR:EMPTY_COMMAND");
        return;
    }

    ESP_LOGI(TAG, "Command: %s", command);

    if (strcmp(command, "HELLO") == 0) {
        showduino_runtime_record_command(command, true);
        respond("READY");
        return;
    }

    if (strcmp(command, "STATUS:REQUEST") == 0) {
        showduino_runtime_record_command(command, true);
        respond_status();
        return;
    }

    if (strcmp(command, "EMERGENCY:STOP") == 0) {
        showduino_gpio_service_all_safe();
        showduino_runtime_latch_emergency();
        showduino_runtime_record_command(command, true);
        respond("STATUS:EMERGENCY_LOCKED");
        return;
    }

    if (strcmp(command, "EMERGENCY:CLEAR") == 0) {
        showduino_runtime_clear_emergency();
        showduino_runtime_record_command(command, true);
        respond("STATUS:EMERGENCY_CLEARED");
        return;
    }

    if (showduino_runtime_is_emergency_latched()) {
        showduino_runtime_record_command(command, false);
        respond("ERROR:EMERGENCY_LOCKED");
        return;
    }

    if (strcmp(command, "SHOW:START") == 0) {
        showduino_runtime_set_state(SHOWDUINO_STATE_RUNNING);
        showduino_runtime_record_command(command, true);
        respond("ACK:SHOW:START");
        return;
    }

    if (strcmp(command, "SHOW:STOP") == 0) {
        showduino_gpio_service_all_safe();
        showduino_runtime_set_state(SHOWDUINO_STATE_STOPPED);
        showduino_runtime_record_command(command, true);
        respond("ACK:SHOW:STOP");
        return;
    }

    if (strcmp(command, "LED:ON") == 0) {
        showduino_gpio_service_set_test_output(true);
        showduino_runtime_record_command(command, true);
        respond("ACK:LED:ON");
        return;
    }

    if (strcmp(command, "LED:OFF") == 0) {
        showduino_gpio_service_set_test_output(false);
        showduino_runtime_record_command(command, true);
        respond("ACK:LED:OFF");
        return;
    }

    if (strcmp(command, "LED:TOGGLE") == 0) {
        showduino_gpio_service_toggle_test_output();
        showduino_runtime_record_command(command, true);
        respond("ACK:LED:TOGGLE");
        return;
    }

    showduino_runtime_record_command(command, false);
    respond("ERROR:UNKNOWN_COMMAND");
}
