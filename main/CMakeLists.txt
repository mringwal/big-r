idf_component_register(SRCS "app_main.cpp" "app_driver.cpp" "app_mqtt.cpp" "mqtt_select.c"
    PRIV_INCLUDE_DIRS "." "${ESP_MATTER_PATH}/examples/common/utils"
    PRIV_REQUIRES lan_ota big_r_led esp_driver_gpio esp_timer nvs_flash esp_matter esp_matter_console espressif__mqtt espressif__cjson esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
