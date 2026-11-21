#pragma once
#include <stdint.h>
#include "color_format.h"

#define BIG_R_MATRIX_SIDE 32
#define BIG_R_MATRIX_PIXELS (BIG_R_MATRIX_SIDE * BIG_R_MATRIX_SIDE)
#define BIG_R_PATTERN_COUNT 4
#define BIG_R_FIRE_MODE 6

/* Render the same 1-based animation IDs and phase used by the device. */
void big_r_animation_render(unsigned mode, unsigned phase, uint8_t brightness,
                            RGB_color_t framebuffer[BIG_R_MATRIX_PIXELS]);
