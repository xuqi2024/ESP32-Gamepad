idf_component_register(
    SRCS "src/system_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos
)
