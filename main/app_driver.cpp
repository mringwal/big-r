/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

#include <esp_matter.h>
#include <app/data-model/Decode.h>
#include <app_priv.h>
#include <common_macros.h>

#include <big_r_led.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <platform/CHIPDeviceLayer.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;

// Global variables to store current XY color coordinates
static uint16_t current_x = 0x5000;
static uint16_t current_y = 0x5400;

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(big_r_led_handle_t handle, esp_matter_attr_val_t *val)
{
    return big_r_led_set_power(handle, val->val.b);
}

static esp_err_t app_driver_light_set_brightness(big_r_led_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
    return big_r_led_set_brightness(handle, value);
}

static esp_err_t app_driver_light_set_hue(big_r_led_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
    return big_r_led_set_hue(handle, value);
}

static esp_err_t app_driver_light_set_saturation(big_r_led_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
    return big_r_led_set_saturation(handle, value);
}

static esp_err_t app_driver_light_set_temperature(big_r_led_handle_t handle, esp_matter_attr_val_t *val)
{
    if (val->val.u16 == 0) return ESP_ERR_INVALID_ARG;
    uint32_t value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
    return big_r_led_set_temperature(handle, value);
}

static esp_err_t app_driver_light_set_xy(big_r_led_handle_t handle, uint16_t x, uint16_t y)
{
    return big_r_led_set_xy(handle, x, y);
}

#if CONFIG_BIG_R_BUTTON_GPIO >= 0
static void next_animation(intptr_t)
{
    bool power = big_r_led_next_animation();
    ESP_LOGI(TAG, "Button selected %s", power ? "animation" : "off");
    esp_matter_attr_val_t val = esp_matter_bool(power);
    attribute::update(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
}

// Poll at 20 ms and require three identical samples for debounce.
static void poll_button(void *)
{
    static int stable = 1;
    static int previous = 1;
    static unsigned samples = 0;
    int level = gpio_get_level((gpio_num_t)CONFIG_BIG_R_BUTTON_GPIO);
    samples = level == previous ? samples + (samples < 3) : 1;
    previous = level;
    if (samples == 3 && stable != level) {
        stable = level;
        if (level == 0) {
            CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(next_animation, 0);
            if (err != CHIP_NO_ERROR) ESP_LOGE(TAG, "Cannot queue button event: %" CHIP_ERROR_FORMAT, err.Format());
        }
    }
}

#endif

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        big_r_led_handle_t handle = (big_r_led_handle_t)driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentX::Id) {
                current_x = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            } else if (attribute_id == ColorControl::Attributes::CurrentY::Id) {
                current_y = val->val.u16;
                err = app_driver_light_set_xy(handle, current_x, current_y);
            }
        }
    }
    if (err == ESP_OK && endpoint_id == light_endpoint_id && cluster_id == ColorControl::Id &&
        (attribute_id == ColorControl::Attributes::CurrentHue::Id ||
         attribute_id == ColorControl::Attributes::CurrentSaturation::Id ||
         attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id ||
         attribute_id == ColorControl::Attributes::CurrentX::Id ||
         attribute_id == ColorControl::Attributes::CurrentY::Id)) {
        // A color request leaves the local off/animation cycle and lights a solid color.
        esp_matter_attr_val_t power;
        attribute::get_val(attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id), &power);
        if (!power.val.b) {
            power.val.b = true;
            err = attribute::update(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &power);
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    big_r_led_handle_t handle = (big_r_led_handle_t)priv_data;
    esp_matter_attr_val_t val;

    /* Setting brightness */
    attribute_t *attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting color */
    attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
        /* Setting hue */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        /* Setting saturation */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
        /* Setting temperature */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentXAndCurrentY) {
        /* Setting XY coordinates */
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentX::Id);
        attribute::get_val(attribute, &val);
        current_x = val.val.u16;
        attribute = attribute::get(endpoint_id, ColorControl::Id, ColorControl::Attributes::CurrentY::Id);
        attribute::get_val(attribute, &val);
        current_y = val.val.u16;
        err |= app_driver_light_set_xy(handle, current_x, current_y);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
    big_r_led_config_t config = {};
    config.gpio = CONFIG_BIG_R_LED_GPIO;
    return big_r_led_init(&config);
}

void app_driver_button_init()
{
#if CONFIG_BIG_R_BUTTON_GPIO >= 0
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << CONFIG_BIG_R_BUTTON_GPIO;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    esp_timer_create_args_t args = {};
    args.callback = poll_button;
    args.name = "big_r_button";
    static esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 20000));
#endif
}

// ESP-Matter skips attribute callbacks when a value is unchanged. A repeated
// color command still needs to leave an animation and restore the solid color.
static void apply_requested_solid(intptr_t endpoint_id)
{
    ESP_ERROR_CHECK(app_driver_light_set_defaults((uint16_t)endpoint_id));
    esp_matter_attr_val_t power = esp_matter_bool(true);
    attribute::update((uint16_t)endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &power);
}

template <typename T>
static bool executes_color_request(const chip::app::ConcreteCommandPath &path, chip::TLV::TLVReader &reader)
{
    T data;
    if (chip::app::DataModel::Decode(reader, data) != CHIP_NO_ERROR || data.transitionTime > 65534) {
        return false;
    }
    using namespace ColorControl::Commands;
    if constexpr (std::is_same_v<T, MoveToHue::DecodableType>) {
        if (data.hue > 254 || static_cast<unsigned>(data.direction) > 3) return false;
    } else if constexpr (std::is_same_v<T, MoveToSaturation::DecodableType>) {
        if (data.saturation > 254) return false;
    } else if constexpr (std::is_same_v<T, MoveToHueAndSaturation::DecodableType>) {
        if (data.hue > 254 || data.saturation > 254) return false;
    } else if constexpr (std::is_same_v<T, MoveToColor::DecodableType>) {
        if (data.colorX > 0xFEFF || data.colorY > 0xFEFF) return false;
    }
    esp_matter_attr_val_t power, options;
    attribute::get_val(attribute::get(path.mEndpointId, OnOff::Id, OnOff::Attributes::OnOff::Id), &power);
    attribute::get_val(attribute::get(path.mEndpointId, ColorControl::Id, ColorControl::Attributes::Options::Id), &options);
    uint8_t effective = (options.val.u8 & ~data.optionsMask.Raw()) |
                        (data.optionsOverride.Raw() & data.optionsMask.Raw());
    return power.val.b || (effective & 1);
}

static esp_err_t color_request_cb(const chip::app::ConcreteCommandPath &path, chip::TLV::TLVReader &reader, void *)
{
    using namespace ColorControl::Commands;
    bool execute = false;
    switch (path.mCommandId) {
    case MoveToHue::Id:
        execute = executes_color_request<MoveToHue::DecodableType>(path, reader); break;
    case MoveToSaturation::Id:
        execute = executes_color_request<MoveToSaturation::DecodableType>(path, reader); break;
    case MoveToHueAndSaturation::Id:
        execute = executes_color_request<MoveToHueAndSaturation::DecodableType>(path, reader); break;
    case MoveToColor::Id:
        execute = executes_color_request<MoveToColor::DecodableType>(path, reader); break;
    case MoveToColorTemperature::Id:
        execute = executes_color_request<MoveToColorTemperature::DecodableType>(path, reader); break;
    }
    if (execute) {
        CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(apply_requested_solid, path.mEndpointId);
        if (err != CHIP_NO_ERROR) ESP_LOGE(TAG, "Cannot queue solid color: %" CHIP_ERROR_FORMAT, err.Format());
    }
    return ESP_OK; // The standard handler decodes and executes the original request.
}

void app_driver_register_color_commands(uint16_t endpoint_id)
{
    using namespace ColorControl::Commands;
    const uint32_t ids[] = {MoveToHue::Id, MoveToSaturation::Id, MoveToHueAndSaturation::Id,
                            MoveToColor::Id, MoveToColorTemperature::Id};
    cluster_t *color = cluster::get(endpoint_id, ColorControl::Id);
    for (uint32_t id : ids) {
        command_t *cmd = command::get(color, id, COMMAND_FLAG_ACCEPTED);
        if (cmd) command::set_user_callback(cmd, color_request_cb);
    }
}
