#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#ifdef __cplusplus
extern "C" {
#endif

// One physical letter R; initialization and task ownership last for the application lifetime.
typedef void *big_r_led_handle_t;
typedef struct {
    int gpio;
} big_r_led_config_t;

big_r_led_handle_t big_r_led_init(big_r_led_config_t *config);
esp_err_t big_r_led_set_power(big_r_led_handle_t handle, bool power);
esp_err_t big_r_led_set_brightness(big_r_led_handle_t handle, uint8_t brightness); // 0..100
// Color setters enable solid output, leaving animation/off mode.
esp_err_t big_r_led_set_hue(big_r_led_handle_t handle, uint16_t hue); // 0..360
esp_err_t big_r_led_set_saturation(big_r_led_handle_t handle, uint8_t saturation); // 0..100
esp_err_t big_r_led_set_temperature(big_r_led_handle_t handle, uint32_t kelvin);
esp_err_t big_r_led_set_xy(big_r_led_handle_t handle, uint16_t x, uint16_t y);
// Cycle solid -> animation 1 -> ... -> animation 4 -> off -> animation 1.
// Returns the resulting power state. Call after initialization, in task context.
bool big_r_led_next_animation(void);
#ifdef __cplusplus
}
#endif
