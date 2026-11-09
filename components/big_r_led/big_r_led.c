/* BIG-R renderer. Original geometry/patterns are CC0; see legacy/big-r.c.
 * Color conversion is adapted from Espressif's official Matter light example.
 */
#include "sdkconfig.h"
#include "big_r_led.h"
#include "color_format.h"
#include "big_r_patterns.h"
#include "led_strip.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

#define MATRIX_PIXELS (32 * 32)
#define PATTERN_COUNT (sizeof(patterns) / sizeof(patterns[0]))
enum { MODE_OFF = 0, MODE_SOLID = 5 };
typedef struct {
    bool power;
    uint8_t brightness;
    RGB_color_t solid;
    HS_color_t hs;
    XY_color_t xy;
    bool use_xy;
    unsigned mode;
} render_state_t;

static led_strip_handle_t strip;
static SemaphoreHandle_t mutex;
static render_state_t state = { .brightness = 25, .hs = {0, 100}, .mode = MODE_SOLID };
static RGB_color_t framebuffer[MATRIX_PIXELS];
static const char *TAG = "big_r_led";

static void render_task(void *arg)
{
    unsigned phase = 0;
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        render_state_t snapshot = state;
        xSemaphoreGive(mutex);
        if (!snapshot.power || snapshot.mode == MODE_OFF) {
            memset(framebuffer, 0, sizeof(framebuffer));
        } else if (snapshot.mode == MODE_SOLID) {
            for (unsigned i = 0; i < MATRIX_PIXELS; ++i) {
                framebuffer[i] = snapshot.solid;
            }
        } else {
            const uint16_t *pattern = patterns[snapshot.mode - 1];
            for (unsigned i = 0; i < MATRIX_PIXELS; ++i) {
                HS_color_t hs = {(pattern[i] + phase) % 360, 100};
                hsv_to_rgb(hs, snapshot.brightness, &framebuffer[i]);
            }
            phase = (phase + 1) % 360;
        }
        // Only mapped LEDs are illuminated. Unmapped gap LEDs stay at zero.
        for (unsigned i = 0; i < sizeof(led_to_matrix) / sizeof(led_to_matrix[0]); ++i) {
            const led_to_matrix_t *entry = &led_to_matrix[i];
            RGB_color_t rgb = framebuffer[entry->matrix_index];
            ESP_ERROR_CHECK(led_strip_set_pixel(strip, entry->led_id, rgb.red, rgb.green, rgb.blue));
        }
        ESP_ERROR_CHECK(led_strip_refresh(strip));
        vTaskDelay(pdMS_TO_TICKS(CONFIG_BIG_R_FRAME_MS));
    }
}

big_r_led_handle_t big_r_led_init(big_r_led_config_t *config)
{
    if (strip || !config) {
        return NULL;
    }
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        return NULL;
    }
    led_strip_config_t led_config = {
        .strip_gpio_num = config->gpio,
        .max_leds = CONFIG_BIG_R_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    // SPI DMA avoids RMT interrupt starvation during Wi-Fi traffic on ESP32-C3.
    // SPI2 is dedicated to the LEDs; no clock pin is connected to the strip.
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    esp_err_t err = led_strip_new_spi_device(&led_config, &spi_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED initialization failed: %s", esp_err_to_name(err));
        vSemaphoreDelete(mutex);
        mutex = NULL;
        return NULL;
    }
    ESP_ERROR_CHECK(led_strip_clear(strip));
    if (xTaskCreate(render_task, "big_r_render", 4096, NULL, 5, NULL) != pdPASS) {
        led_strip_del(strip);
        strip = NULL;
        vSemaphoreDelete(mutex);
        mutex = NULL;
        return NULL;
    }
    return strip;
}

bool big_r_led_next_animation(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.mode = state.mode == MODE_SOLID ? 1 :
        (state.mode >= PATTERN_COUNT ? MODE_OFF : state.mode + 1);
    // A press from solid color starts the first animation.
    // MODE_SOLID is outside the animation cycle.
    state.power = state.mode != MODE_OFF;
    bool power = state.power;
    xSemaphoreGive(mutex);
    return power;
}

esp_err_t big_r_led_set_power(big_r_led_handle_t handle, bool power)
{
    if (handle != strip || !strip) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.power = power;
    if (power && state.mode == MODE_OFF) state.mode = MODE_SOLID;
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t big_r_led_set_brightness(big_r_led_handle_t handle, uint8_t brightness)
{
    if (handle != strip || !strip || brightness > 100) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    // Preserve the active color space when adjusting brightness.
    state.brightness = brightness;
    if (state.use_xy) {
        xy_to_rgb(state.xy, 255 * brightness / 100, &state.solid);
    } else {
        hsv_to_rgb(state.hs, brightness, &state.solid);
    }
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t big_r_led_set_hue(big_r_led_handle_t handle, uint16_t hue)
{
    if (handle != strip || !strip || hue > 360) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.use_xy = false;
    state.hs.hue = hue;
    state.mode = MODE_SOLID;
    state.power = true;
    hsv_to_rgb(state.hs, state.brightness, &state.solid);
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t big_r_led_set_saturation(big_r_led_handle_t handle, uint8_t saturation)
{
    if (handle != strip || !strip || saturation > 100) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.use_xy = false;
    state.hs.saturation = saturation;
    state.mode = MODE_SOLID;
    state.power = true;
    hsv_to_rgb(state.hs, state.brightness, &state.solid);
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t big_r_led_set_temperature(big_r_led_handle_t handle, uint32_t temperature)
{
    if (handle != strip || !strip || !temperature) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.use_xy = false;
    temp_to_hs(temperature, &state.hs);
    state.mode = MODE_SOLID;
    state.power = true;
    hsv_to_rgb(state.hs, state.brightness, &state.solid);
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t big_r_led_set_xy(big_r_led_handle_t handle, uint16_t x, uint16_t y)
{
    if (handle != strip || !strip) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.use_xy = true;
    state.xy = (XY_color_t){x, y};
    xy_to_rgb(state.xy, 255 * state.brightness / 100, &state.solid);
    state.mode = MODE_SOLID;
    state.power = true;
    xSemaphoreGive(mutex);
    return ESP_OK;
}
