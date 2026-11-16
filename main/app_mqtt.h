#pragma once
#include <stdint.h>
#include <esp_err.h>
#include <big_r_led.h>

esp_err_t app_mqtt_start(big_r_led_handle_t light);
void app_mqtt_publish_state();
void app_driver_select_mode(intptr_t mode);
