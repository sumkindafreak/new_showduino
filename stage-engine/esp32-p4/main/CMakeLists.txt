idf_component_register(
    SRCS
        "main.c"
        "showduino_runtime.c"
        "showduino_command_router.c"
        "showduino_gpio_service.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)
