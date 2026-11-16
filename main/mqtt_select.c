#include "mqtt_select.h"
#include <string.h>

const char *mqtt_select_state(big_r_animation_t mode, bool power)
{
    if (!power || mode == BIG_R_ANIMATION_OFF) return "Off";
    if (mode == BIG_R_ANIMATION_SOLID) return "Solid";
    size_t count;
    const big_r_animation_info_t *list = big_r_led_get_animations(&count);
    for (size_t i = 0; i < count; ++i) if (list[i].id == mode) return list[i].name;
    return NULL;
}

bool mqtt_select_parse(const char *data, size_t len, big_r_animation_t *mode)
{
    if (!data || !mode) return false;
    if (len == 3 && memcmp(data, "Off", 3) == 0) { *mode = BIG_R_ANIMATION_OFF; return true; }
    if (len == 5 && memcmp(data, "Solid", 5) == 0) { *mode = BIG_R_ANIMATION_SOLID; return true; }
    size_t count;
    const big_r_animation_info_t *list = big_r_led_get_animations(&count);
    for (size_t i = 0; i < count; ++i) {
        if (strlen(list[i].name) == len && memcmp(data, list[i].name, len) == 0) {
            *mode = list[i].id;
            return true;
        }
    }
    return false;
}
