#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "showduino_command_router.h"
#include "showduino_gpio_service.h"
#include "showduino_runtime.h"

static const char *TAG = "showduino_p4";

static void console_response(const char *response, void *context) {
    (void)context;
    printf("%s\n", response);
    fflush(stdout);
}

static void print_hardware_summary(void) {
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "Showduino Stage Engine booting");
    ESP_LOGI(TAG, "Target: ESP32-P4");
    ESP_LOGI(TAG, "CPU cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Silicon revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "Flash: %lu MB", (unsigned long)(flash_size / (1024UL * 1024UL)));
    ESP_LOGI(TAG, "Reset reason: %d", esp_reset_reason());
}

static void stage_engine_console_task(void *argument) {
    (void)argument;

    char line[128];

    ESP_LOGI(TAG, "Console command interface ready");
    ESP_LOGI(TAG, "Commands: HELLO, STATUS:REQUEST, LED:ON, LED:OFF, LED:TOGGLE");
    ESP_LOGI(TAG, "Safety: EMERGENCY:STOP, EMERGENCY:CLEAR, SHOW:START, SHOW:STOP");

    while (true) {
        if (fgets(line, sizeof(line), stdin) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '\0') {
                showduino_command_router_handle(line);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void stage_engine_status_task(void *argument) {
    (void)argument;

    while (true) {
        const showduino_runtime_status_t status = showduino_runtime_get_status();

        ESP_LOGI(
            TAG,
            "State=%s emergency=%s accepted=%lu rejected=%lu last=%s",
            showduino_runtime_state_name(status.state),
            status.emergency_latched ? "YES" : "NO",
            (unsigned long)status.commands_received,
            (unsigned long)status.commands_rejected,
            status.last_command[0] != '\0' ? status.last_command : "NONE"
        );

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void) {
    print_hardware_summary();

    showduino_runtime_init();

    ESP_ERROR_CHECK(showduino_gpio_service_init());
    showduino_command_router_init(console_response, NULL);

    showduino_runtime_set_state(SHOWDUINO_STATE_READY);
    ESP_LOGI(TAG, "Stage Engine state: READY");

    xTaskCreate(
        stage_engine_console_task,
        "showduino_console",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        stage_engine_status_task,
        "showduino_status",
        4096,
        NULL,
        2,
        NULL
    );
}
