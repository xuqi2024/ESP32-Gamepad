idf_component_register(
    SRCS "src/task_scheduler.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer
)
