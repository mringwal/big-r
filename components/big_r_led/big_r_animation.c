#include "big_r_animation.h"
#include "big_r_patterns.h"
#include <string.h>

_Static_assert(sizeof(patterns) / sizeof(patterns[0]) == BIG_R_PATTERN_COUNT, "Pattern count mismatch");

void big_r_animation_render(unsigned mode, unsigned phase, uint8_t brightness,
                            RGB_color_t framebuffer[BIG_R_MATRIX_PIXELS])
{
    if (mode == 0 || mode > BIG_R_PATTERN_COUNT) {
        memset(framebuffer, 0, sizeof(*framebuffer) * BIG_R_MATRIX_PIXELS);
        return;
    }
    const uint16_t *pattern = patterns[mode - 1];
    for (unsigned i = 0; i < BIG_R_MATRIX_PIXELS; ++i) {
        HS_color_t hs = {(pattern[i] + phase % 360) % 360, 100};
        hsv_to_rgb(hs, brightness, &framebuffer[i]);
    }
}
