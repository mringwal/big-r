#!/usr/bin/env python3
"""Compile the real renderer against a minimal synchronous ESP-IDF test backend."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sdk = r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#define CONFIG_BIG_R_LED_COUNT 573
#define CONFIG_BIG_R_FRAME_MS 20
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERROR_CHECK(x) assert((x) == 0)
#define ESP_LOGE(...) ((void)0)
#define pdPASS 1
#define portMAX_DELAY 0xffffffff
#define pdMS_TO_TICKS(x) (x)
#define SPI_CLK_SRC_DEFAULT 0
#define SPI2_HOST 1
#define LED_MODEL_WS2812 0
#define LED_STRIP_COLOR_COMPONENT_FMT_GRB 0
static inline const char *esp_err_to_name(int err) { return "test"; }
typedef int esp_err_t;
typedef void *SemaphoreHandle_t;
typedef void *led_strip_handle_t;
typedef struct { int strip_gpio_num; unsigned max_leds; int led_model; int color_component_format; } led_strip_config_t;
typedef struct { int clk_src; int spi_bus; struct { unsigned with_dma; } flags; } led_strip_spi_config_t;
void *xSemaphoreCreateMutex(void);
int xSemaphoreTake(void *, unsigned);
int xSemaphoreGive(void *);
void vSemaphoreDelete(void *);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
void vTaskDelay(unsigned);
int led_strip_new_spi_device(const led_strip_config_t *, const led_strip_spi_config_t *, void **);
int led_strip_set_pixel(void *, unsigned, unsigned, unsigned, unsigned);
int led_strip_refresh(void *);
int led_strip_clear(void *);
int led_strip_del(void *);
'''
with tempfile.TemporaryDirectory(prefix="big-r-test-") as temporary:
    tmp = Path(temporary)
    (tmp / "fake_sdk.h").write_text(sdk)
    for header in ["sdkconfig.h", "esp_err.h", "esp_log.h", "led_strip.h", "driver/spi_master.h",
                   "freertos/FreeRTOS.h", "freertos/task.h", "freertos/semphr.h"]:
        path = tmp / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "fake_sdk.h"\n')
    component = root / "components/big_r_led"
    subprocess.run(["cc", "-std=c11", "-fsanitize=address,undefined", "-g",
                    "-I" + str(tmp), "-I" + str(component / "include"), "-I" + str(component),
                    str(root / "tests/test_renderer.c"), str(component / "big_r_led.c"),
                    str(component / "color_format.c"), "-lm", "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
