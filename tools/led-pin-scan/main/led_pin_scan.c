#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "soc/gpio_sig_map.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_COUNT 573
#define BUTTON_GPIO 10
static const int pins[] = {0,1,2,3,4,5,6,7,8,9,18,19};
static uint8_t pixels[LED_COUNT * 3];
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
static unsigned lit_leds = 1;
static int current_pin = -1;
static bool quit;
static bool running;

static void send_frame(bool on)
{
    memset(pixels, 0, sizeof(pixels));
    if (on) memset(pixels, 26, lit_leds * 3); // GRB white at approximately 10%.
    // GPIO direction/output-enable APIs reset the peripheral matrix route.
    // The RMT driver owns output configuration for the entire candidate test.
    gpio_io_config_t io;
    ESP_ERROR_CHECK(gpio_get_io_config(current_pin, &io));
    if (io.sig_out != RMT_SIG_OUT0_IDX && io.sig_out != RMT_SIG_OUT1_IDX) {
        printf("ERROR GPIO%d lost RMT routing: signal %lu\n", current_pin, (unsigned long)io.sig_out);
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }
    rmt_transmit_config_t config = {.loop_count = 0, .flags.eot_level = 0};
    ESP_ERROR_CHECK(rmt_transmit(channel, encoder, pixels, sizeof(pixels), &config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(channel, 1000));
    // Keep the RMT route intact and hold its end-of-transfer low level for the latch.
    vTaskDelay(pdMS_TO_TICKS(10));
}

static char command(void)
{
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return 0;
    if (c == 'q') { quit = true; running = false; }
    if (c == '1') { lit_leds = 1; printf("MODE: first LED only at 10%% white\n"); }
    if (c == '5') { lit_leds = 50; printf("MODE: first 50 LEDs at 10%% white\n"); }
    if (c == 'a') { lit_leds = LED_COUNT; printf("MODE: all %u LEDs at 10%% white -- external LED power required\n", lit_leds); }
    if (c == 'g') running = true;
    return c;
}

static void release_pin(void)
{
    send_frame(false);
    ESP_ERROR_CHECK(rmt_disable(channel));
    ESP_ERROR_CHECK(rmt_del_channel(channel));
    channel = NULL;
    ESP_ERROR_CHECK(gpio_set_direction(current_pin, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(current_pin, GPIO_PULLDOWN_ONLY));
    current_pin = -1;
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    gpio_config_t button = {.pin_bit_mask=1ULL<<BUTTON_GPIO, .mode=GPIO_MODE_INPUT, .pull_up_en=GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&button));
    rmt_bytes_encoder_config_t encoding = {
        .bit0 = {.level0=1, .duration0=4, .level1=0, .duration1=9},
        .bit1 = {.level0=1, .duration0=8, .level1=0, .duration1=5},
        .flags.msb_first = true,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&encoding, &encoder));
    puts("\nBIG-R LED PIN SCANNER -- RAM ONLY, FLASH UNCHANGED");
    puts("Pins: 0..9,18,19. Skipped: button 10, flash 11..17, UART 20..21.");
    puts("Commands: 1=first LED, 5=first 50, a=all LEDs (external power), g=start/resume, q=off/stop.");
    puts("WAITING: select mode, then send g. Press the installed button WHILE THE LIGHT IS ON to lock that GPIO.");
    while (!running && !quit) { command(); vTaskDelay(pdMS_TO_TICKS(20)); }
    while (!quit) {
        for (unsigned i=0; i<sizeof(pins)/sizeof(pins[0]) && !quit; ++i) {
            current_pin = pins[i];
            rmt_tx_channel_config_t tx = {
                .gpio_num = current_pin, .clk_src = RMT_CLK_SRC_DEFAULT,
                .resolution_hz = 10000000, .mem_block_symbols = 64, .trans_queue_depth = 1,
            };
            ESP_ERROR_CHECK(rmt_new_tx_channel(&tx, &channel));
            ESP_ERROR_CHECK(gpio_set_drive_capability(current_pin, GPIO_DRIVE_CAP_0));
            ESP_ERROR_CHECK(rmt_enable(channel));
            gpio_io_config_t io;
            ESP_ERROR_CHECK(gpio_get_io_config(current_pin, &io));
            printf("ROUTE GPIO%d: signal=%lu (expected RMT %d/%d), output=%d, peripheral_oe=%d\n",
                current_pin, (unsigned long)io.sig_out, RMT_SIG_OUT0_IDX, RMT_SIG_OUT1_IDX, io.oe, io.oe_ctrl_by_periph);
            send_frame(false);
            printf("SCAN GPIO%d ON: %u LED(s), white 10%% -- press button now if visible\n", current_pin, lit_leds);
            send_frame(true);
            unsigned low_samples = 0;
            bool locked = false;
            for (unsigned tick=0; (tick<400 || locked) && !quit; ++tick) {
                char c = command();
                if (c == 'g' && locked) { locked=false; break; }
                low_samples = gpio_get_level(BUTTON_GPIO) == 0 ? low_samples+1 : 0;
                if (low_samples==5 && !locked) {
                    locked=true;
                    printf("FOUND/LOCKED GPIO%d -- selected by button. Holding 10%% output; g resumes, q turns off.\n", current_pin);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            printf("SCAN GPIO%d OFF\n", current_pin);
            release_pin();
            for (unsigned tick=0; tick<100 && !quit; ++tick) { command(); vTaskDelay(pdMS_TO_TICKS(10)); }
        }
        if (!quit) puts("SCAN round complete; repeating.");
    }
    ESP_ERROR_CHECK(rmt_del_encoder(encoder));
    puts("STOPPED: last pin cleared and released; reset returns to flashed firmware.");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
