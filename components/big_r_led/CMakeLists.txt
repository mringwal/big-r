idf_component_register(SRCS "big_r_led.c" "color_format.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES led_strip esp_driver_spi freertos)
