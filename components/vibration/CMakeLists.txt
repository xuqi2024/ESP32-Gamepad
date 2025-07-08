idf_component_register(
    SRCS "src/vibration.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_timer
        bluetooth_hid
)
