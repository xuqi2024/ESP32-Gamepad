# 注册main组件
idf_component_register(
    SRCS "main.c" 
         "gamepad_controller.c"
    INCLUDE_DIRS "."
    REQUIRES 
        bt
        esp_hid
        nvs_flash
        driver
        bluetooth_hid
        device_control
        vibration
)
