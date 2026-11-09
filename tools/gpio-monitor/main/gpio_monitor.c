#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const int pins[] = {0,1,2,3,4,5,6,7,8,9,10,18,19};
#define N_PINS (sizeof(pins) / sizeof(pins[0]))
static int stable[N_PINS], previous[N_PINS], samples[N_PINS];
static const char *pull_name = "UP";

static void set_pulls(gpio_pull_mode_t mode, const char *name)
{
    pull_name = name;
    for (unsigned i = 0; i < N_PINS; ++i) {
        ESP_ERROR_CHECK(gpio_set_pull_mode(pins[i], mode));
        samples[i] = 0;
    }
    printf("\nPULL MODE: %s (wait 1 second, then press and release the button several times)\n", name);
}

static void snapshot(void)
{
    printf("STATE t=%" PRId64 "ms pulls=%s", esp_timer_get_time()/1000, pull_name);
    for (unsigned i = 0; i < N_PINS; ++i) printf(" GPIO%d=%d", pins[i], gpio_get_level(pins[i]));
    printf("\n");
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    uint64_t mask = 0;
    for (unsigned i = 0; i < N_PINS; ++i) mask |= 1ULL << pins[i];
    gpio_config_t config = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    puts("\nBIG-R GPIO MONITOR -- RUNNING FROM RAM, FLASH UNCHANGED");
    puts("Watching GPIO0-10 and GPIO18-19. GPIO11-17: flash, GPIO20-21: UART, left untouched.");
    puts("Commands: u = pull-ups, d = pull-downs, f = floating, s = snapshot. Reset restores original firmware.");
    for (unsigned i = 0; i < N_PINS; ++i) stable[i] = previous[i] = gpio_get_level(pins[i]);
    snapshot();
    unsigned loops = 0;
    for (;;) {
        char cmd;
        if (read(STDIN_FILENO, &cmd, 1) == 1) {
            if (cmd == 'u') set_pulls(GPIO_PULLUP_ONLY, "UP");
            if (cmd == 'd') set_pulls(GPIO_PULLDOWN_ONLY, "DOWN");
            if (cmd == 'f') set_pulls(GPIO_FLOATING, "FLOATING");
            if (cmd == 's') snapshot();
        }
        for (unsigned i = 0; i < N_PINS; ++i) {
            int level = gpio_get_level(pins[i]);
            samples[i] = level == previous[i] ? samples[i] + (samples[i] < 3) : 1;
            previous[i] = level;
            if (samples[i] == 3 && level != stable[i]) {
                printf("EVENT t=%" PRId64 "ms GPIO%d %d -> %d (stable >=40ms)\n",
                    esp_timer_get_time()/1000, pins[i], stable[i], level);
                stable[i] = level;
            }
        }
        if (++loops % 100 == 0) snapshot();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
