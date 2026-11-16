#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <big_r_led.h>

#ifdef __cplusplus
extern "C" {
#endif
const char *mqtt_select_state(big_r_animation_t mode, bool power);
bool mqtt_select_parse(const char *data, size_t len, big_r_animation_t *mode);
#ifdef __cplusplus
}
#endif
