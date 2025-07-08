idf_component_register(
    SRCS "src/bluetooth_hid.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        bt
        esp_bt_bluedroid
        driver
        esp_timer
        esp_common
)
