#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <esp_err.h>
#ifdef __cplusplus
extern "C" {
#endif

// One physical letter R; initialization and task ownership last for the application lifetime.
typedef void *big_r_led_handle_t;
typedef enum {
    BIG_R_ANIMATION_OFF = 0,
    BIG_R_ANIMATION_RAINBOW_HORIZONTAL,
    BIG_R_ANIMATION_RAINBOW_VERTICAL,
    BIG_R_ANIMATION_RAINBOW_QUADRANT,
    BIG_R_ANIMATION_RAINBOW_CENTERED,
    BIG_R_ANIMATION_SOLID,
    BIG_R_ANIMATION_FIRE,
} big_r_animation_t;

typedef struct {
    big_r_animation_t id;
    const char *name;
} big_r_animation_info_t;
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
// Returns a permanent, read-only list of selectable animations (excludes off and solid).
const big_r_animation_info_t *big_r_led_get_animations(size_t *count);
// Reports the selected mode, independently of the Matter power gate.
esp_err_t big_r_led_get_animation(big_r_led_handle_t handle, big_r_animation_t *animation);
// Selects a listed animation and enables output; off and solid are not accepted.
esp_err_t big_r_led_set_animation(big_r_led_handle_t handle, big_r_animation_t animation);
// Atomically read the selected mode and power gate.
esp_err_t big_r_led_get_state(big_r_led_handle_t handle, big_r_animation_t *animation, bool *power);
// Restore the last solid color and enable output.
esp_err_t big_r_led_select_solid(big_r_led_handle_t handle);
// Cycle solid -> animation 1 -> ... -> animation 4 -> fire -> off -> animation 1.
// Returns the resulting power state. Call after initialization, in task context.
bool big_r_led_next_animation(void);
#ifdef __cplusplus
}
#endif
