#include "showduino_gpio_service.h"

#include "driver/gpio.h"
#include "esp_log.h"

// Temporary proof output using an exposed, programmable P4 GPIO from the board pinout.
// This can be changed in one place after the exact test LED/output is chosen.
#define SHOWDUINO_TEST_OUTPUT_GPIO GPIO_NUM_23
#define SHOWDUINO_TEST_OUTPUT_ON   1
#define SHOWDUINO_TEST_OUTPUT_OFF  0

static const char *TAG = "showduino_gpio";
static bool s_test_output_on = false;

esp_err_t showduino_gpio_service_init(void) {
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << SHOWDUINO_TEST_OUTPUT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure test GPIO %d: %s",
                 SHOWDUINO_TEST_OUTPUT_GPIO, esp_err_to_name(result));
        return result;
    }

    s_test_output_on = false;
    gpio_set_level(SHOWDUINO_TEST_OUTPUT_GPIO, SHOWDUINO_TEST_OUTPUT_OFF);
    ESP_LOGI(TAG, "Test output ready on GPIO %d", SHOWDUINO_TEST_OUTPUT_GPIO);
    return ESP_OK;
}

esp_err_t showduino_gpio_service_set_test_output(bool on) {
    esp_err_t result = gpio_set_level(
        SHOWDUINO_TEST_OUTPUT_GPIO,
        on ? SHOWDUINO_TEST_OUTPUT_ON : SHOWDUINO_TEST_OUTPUT_OFF
    );

    if (result == ESP_OK) {
        s_test_output_on = on;
        ESP_LOGI(TAG, "Test output is %s", on ? "ON" : "OFF");
    }

    return result;
}

esp_err_t showduino_gpio_service_toggle_test_output(void) {
    return showduino_gpio_service_set_test_output(!s_test_output_on);
}

bool showduino_gpio_service_get_test_output(void) {
    return s_test_output_on;
}

void showduino_gpio_service_all_safe(void) {
    showduino_gpio_service_set_test_output(false);
    ESP_LOGW(TAG, "All GPIO services forced to safe state");
}
