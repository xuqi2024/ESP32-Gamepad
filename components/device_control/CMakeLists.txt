idf_component_register(
    SRCS "src/car_control.c"
         "src/plane_control.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        driver
        esp_timer
)
