#include "big_r_animation.h"
#include "big_r_patterns.h"
#include <string.h>

_Static_assert(sizeof(patterns) / sizeof(patterns[0]) == BIG_R_PATTERN_COUNT, "Pattern count mismatch");

/* Smooth, deterministic noise moves upward through a height-dependent heat field.
 * Integer arithmetic keeps the desktop and ESP32 renderers identical. */
static unsigned noise_point(unsigned x, unsigned y)
{
    uint32_t n = x * 0x8da6b343u + y * 0xd8163841u + 0x9e3779b9u;
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15; n *= 0x846ca68bu; n ^= n >> 16;
    return n & 255u;
}

static unsigned fire_noise(unsigned x, unsigned y)
{
    unsigned fx = x & 255u, fy = y & 255u;
    unsigned a = (noise_point(x >> 8, y >> 8) * (256 - fx) + noise_point((x >> 8) + 1, y >> 8) * fx) >> 8;
    unsigned b = (noise_point(x >> 8, (y >> 8) + 1) * (256 - fx) + noise_point((x >> 8) + 1, (y >> 8) + 1) * fx) >> 8;
    return (a * (256 - fy) + b * fy) >> 8;
}

static void render_fire(unsigned frame, uint8_t brightness, RGB_color_t *pixels)
{
    for (unsigned y = 0; y < 32; ++y) {
        for (unsigned x = 0; x < 32; ++x) {
            /* y=0 is the top. Noise travels upwards as time advances. */
            unsigned noise = fire_noise(x * 64, y * 80 + frame * 18u);
            int heat = (int)noise + (int)y * 6 - 105;
            if (heat < 0) heat = 0;
            if (heat > 255) heat = 255;
            unsigned red = heat < 85 ? (unsigned)heat * 3 : 255;
            unsigned green = heat < 85 ? 0 : heat < 170 ? (unsigned)(heat - 85) * 2 : 170 + (unsigned)(heat - 170);
            unsigned blue = heat > 210 ? (unsigned)(heat - 210) * 3 : 0;
            pixels[y * 32 + x] = (RGB_color_t){red * brightness / 100, green * brightness / 100, blue * brightness / 100};
        }
    }
}

void big_r_animation_render(unsigned mode, unsigned phase, uint8_t brightness,
                            RGB_color_t framebuffer[BIG_R_MATRIX_PIXELS])
{
    if (mode == BIG_R_FIRE_MODE) {
        render_fire(phase, brightness, framebuffer);
        return;
    }
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
