#ifndef SHOWDUINO_GPIO_SERVICE_H
#define SHOWDUINO_GPIO_SERVICE_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t showduino_gpio_service_init(void);
esp_err_t showduino_gpio_service_set_test_output(bool on);
esp_err_t showduino_gpio_service_toggle_test_output(void);
bool showduino_gpio_service_get_test_output(void);
void showduino_gpio_service_all_safe(void);

#ifdef __cplusplus
}
#endif

#endif
