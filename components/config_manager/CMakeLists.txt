idf_component_register(
    SRCS "src/config_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash spiffs
)
