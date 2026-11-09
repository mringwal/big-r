/* Exercise the actual driver through a synchronous fake LED/RTOS backend. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "led_strip.h"
#include "big_r_led.h"
#include "color_format.h"
#include "../components/big_r_led/big_r_patterns.h"
static RGB_color_t pixels[CONFIG_BIG_R_LED_COUNT];
static void (*task_fn)(void *);
static jmp_buf frame_done;
static unsigned remaining_frames;
void *xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(void *s, unsigned t) { return 1; }
int xSemaphoreGive(void *s) { return 1; }
void vSemaphoreDelete(void *s) {}
int xTaskCreate(void (*fn)(void *), const char *name, unsigned size, void *arg, unsigned pri, void *out)
{ task_fn = fn; return 1; }
void vTaskDelay(unsigned ticks) { if (--remaining_frames == 0) longjmp(frame_done, 1); }
int led_strip_new_spi_device(const led_strip_config_t *l, const led_strip_spi_config_t *s, void **out)
{ assert(l->max_leds == 573); assert(s->flags.with_dma); *out = (void *)2; return 0; }
int led_strip_set_pixel(void *s, unsigned i, unsigned r, unsigned g, unsigned b)
{ assert(i < 573); pixels[i] = (RGB_color_t){r,g,b}; return 0; }
int led_strip_refresh(void *s) { return 0; }
int led_strip_clear(void *s) { memset(pixels, 0, sizeof(pixels)); return 0; }
int led_strip_del(void *s) { return 0; }
static void frames(unsigned count) { remaining_frames = count; if (setjmp(frame_done) == 0) task_fn(NULL); }
static void frame(void) { frames(1); }
static bool dark(void) {
    for (unsigned i = 0; i < 573; ++i) if (pixels[i].red || pixels[i].green || pixels[i].blue) return false;
    return true;
}
static void mapped_color(unsigned r, unsigned g, unsigned b) {
    bool mapped[573] = {0};
    for (unsigned i = 0; i < sizeof(led_to_matrix)/sizeof(*led_to_matrix); ++i) {
        unsigned id = led_to_matrix[i].led_id;
        assert(id < 573 && !mapped[id]);
        assert(led_to_matrix[i].matrix_index < 1024);
        mapped[id] = true;
        assert(pixels[id].red == r && pixels[id].green == g && pixels[id].blue == b);
    }
    for (unsigned i = 0; i < 573; ++i) if (!mapped[i])
        assert(pixels[i].red == 0 && pixels[i].green == 0 && pixels[i].blue == 0);
}
int main(void) {
    big_r_led_config_t cfg = {.gpio=18};
    void *h = big_r_led_init(&cfg);
    assert(h && task_fn);
    frame(); assert(dark());
    assert(big_r_led_set_brightness(h, 100) == 0);
    assert(big_r_led_set_hue(h, 0) == 0);
    assert(big_r_led_set_saturation(h, 100) == 0);
    frame(); mapped_color(255,0,0);
    // Four distinct pattern frames must match the original geometry and HSV tables.
    for (unsigned p = 0; p < 4; ++p) {
        assert(big_r_led_next_animation()); frame(); assert(!dark());
        for (unsigned i = 0; i < sizeof(led_to_matrix)/sizeof(*led_to_matrix); ++i) {
            RGB_color_t expected;
            HS_color_t hs = {patterns[p][led_to_matrix[i].matrix_index],100};
            hsv_to_rgb(hs,100,&expected);
            assert(memcmp(&expected,&pixels[led_to_matrix[i].led_id],sizeof(expected)) == 0);
        }
    }
    // A second consecutive render advances the animation hue by one degree.
    frames(2);
    for (unsigned i = 0; i < sizeof(led_to_matrix)/sizeof(*led_to_matrix); ++i) {
        RGB_color_t expected;
        HS_color_t hs = {(patterns[3][led_to_matrix[i].matrix_index] + 1) % 360,100};
        hsv_to_rgb(hs,100,&expected);
        assert(memcmp(&expected,&pixels[led_to_matrix[i].led_id],sizeof(expected)) == 0);
    }
    assert(!big_r_led_next_animation()); frame(); assert(dark());
    assert(big_r_led_set_hue(h,120) == 0); frame(); mapped_color(0,255,0);
    assert(big_r_led_next_animation()); frame(); assert(!dark());
    // Matter color exits animation; power-off and local off can both be left by color.
    assert(big_r_led_set_hue(h,120) == 0); frame(); mapped_color(0,255,0);
    assert(big_r_led_set_power(h,false) == 0); frame(); assert(dark());
    assert(big_r_led_set_hue(h,240) == 0); frame(); mapped_color(0,0,255);
    assert(big_r_led_set_brightness(h,0) == 0); frame(); assert(dark());
    assert(big_r_led_set_brightness(h,100) == 0); frame(); mapped_color(0,0,255);
    // Dimming an XY color must keep XY rather than reverting to the previous hue.
    assert(big_r_led_set_xy(h,30000,20000) == 0);
    assert(big_r_led_set_brightness(h,50) == 0); frame();
    RGB_color_t expected; xy_to_rgb((XY_color_t){30000,20000},127,&expected);
    mapped_color(expected.red,expected.green,expected.blue);
    assert(big_r_led_set_hue(NULL,0) != 0);
    assert(big_r_led_set_brightness(h,101) != 0);
    puts("PASS: geometry, four patterns, button/off cycle, Matter solid colors, power, zero brightness, XY dimming");
}
