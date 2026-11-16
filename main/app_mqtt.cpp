#include <atomic>
#include <cstdio>
#include <cstring>

#include <cJSON.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <mqtt_client.h>
#include <platform/CHIPDeviceLayer.h>

#include "app_mqtt.h"
#include "mqtt_select.h"

static const char *TAG = "big_r_mqtt";
static esp_mqtt_client_handle_t client;
static big_r_led_handle_t light;
static std::atomic<bool> connected{false};
static char device_id[32], command_topic[64], state_topic[64], availability_topic[64], discovery_topic[96];

static bool topic_is(const esp_mqtt_event_handle_t event, const char *topic)
{
    return event->topic && event->topic_len == static_cast<int>(strlen(topic)) &&
           memcmp(event->topic, topic, event->topic_len) == 0;
}

static void publish(const char *topic, const char *payload)
{
    if (esp_mqtt_client_enqueue(client, topic, payload, 0, 1, 1, true) < 0)
        ESP_LOGW(TAG, "Could not queue MQTT publication to %s", topic);
}

void app_mqtt_publish_state()
{
    if (!connected.load() || !light) return;
    big_r_animation_t mode;
    bool power;
    if (big_r_led_get_state(light, &mode, &power) != ESP_OK) return;
    const char *value = mqtt_select_state(mode, power);
    if (value) publish(state_topic, value);
}

static void publish_state_work(intptr_t)
{
    app_mqtt_publish_state();
}

static void publish_discovery()
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON *options = cJSON_AddArrayToObject(root, "options");
    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON *identifiers = device ? cJSON_AddArrayToObject(device, "identifiers") : nullptr;
    bool ok = options && device && identifiers &&
        cJSON_AddStringToObject(root, "name", "Animation") &&
        cJSON_AddStringToObject(root, "unique_id", device_id) &&
        cJSON_AddStringToObject(root, "command_topic", command_topic) &&
        cJSON_AddStringToObject(root, "state_topic", state_topic) &&
        cJSON_AddStringToObject(root, "availability_topic", availability_topic) &&
        cJSON_AddStringToObject(device, "name", "BIG-R") &&
        cJSON_AddStringToObject(device, "manufacturer", "BIG-R") &&
        cJSON_AddItemToArray(identifiers, cJSON_CreateString(device_id));
    size_t count = 0;
    const big_r_animation_info_t *animations = big_r_led_get_animations(&count);
    for (size_t i = 0; ok && i < count; ++i)
        ok = cJSON_AddItemToArray(options, cJSON_CreateString(animations[i].name));
    if (ok) ok = cJSON_AddItemToArray(options, cJSON_CreateString("Solid"));
    if (ok) ok = cJSON_AddItemToArray(options, cJSON_CreateString("Off"));
    char *json = ok ? cJSON_PrintUnformatted(root) : nullptr;
    if (json) {
        publish(discovery_topic, json);
        cJSON_free(json);
    } else {
        ESP_LOGE(TAG, "Could not create MQTT discovery message");
    }
    cJSON_Delete(root);
}

static void schedule(void (*work)(intptr_t), intptr_t arg)
{
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(work, arg);
    if (err != CHIP_NO_ERROR) ESP_LOGE(TAG, "Cannot queue MQTT work: %" CHIP_ERROR_FORMAT, err.Format());
}

static void mqtt_event(void *, esp_event_base_t, int32_t event_id, void *event_data)
{
    auto event = static_cast<esp_mqtt_event_handle_t>(event_data);
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        connected.store(true);
        ESP_LOGI(TAG, "Connected to MQTT broker");
        esp_mqtt_client_subscribe(client, command_topic, 1);
        esp_mqtt_client_subscribe(client, "homeassistant/status", 1);
        publish_discovery();
        schedule(publish_state_work, 0);
        publish(availability_topic, "online");
        break;
    case MQTT_EVENT_DISCONNECTED:
        connected.store(false);
        ESP_LOGW(TAG, "MQTT disconnected; reconnecting");
        break;
    case MQTT_EVENT_DATA:
        // Ignore fragmented messages and retained commands rather than replaying old selections.
        if (event->current_data_offset != 0 || event->data_len != event->total_data_len) break;
        if (topic_is(event, command_topic)) {
            big_r_animation_t mode;
            if (!event->retain && mqtt_select_parse(event->data, event->data_len, &mode))
                schedule(app_driver_select_mode, static_cast<intptr_t>(mode));
            else ESP_LOGW(TAG, "Ignoring invalid or retained animation command");
        } else if (topic_is(event, "homeassistant/status") && event->data_len == 6 &&
                   memcmp(event->data, "online", 6) == 0) {
            publish_discovery();
            schedule(publish_state_work, 0);
        }
        break;
    default:
        break;
    }
}

esp_err_t app_mqtt_start(big_r_led_handle_t handle)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    uint8_t mac[6];
    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) return err;
    snprintf(device_id, sizeof(device_id), "big_r_%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(command_topic, sizeof(command_topic), "%s/animation/set", device_id);
    snprintf(state_topic, sizeof(state_topic), "%s/animation/state", device_id);
    snprintf(availability_topic, sizeof(availability_topic), "%s/availability", device_id);
    snprintf(discovery_topic, sizeof(discovery_topic), "homeassistant/select/%s/config", device_id);
    light = static_cast<big_r_led_handle_t>(handle);
    esp_mqtt_client_config_t config = {};
    config.broker.address.uri = "mqtt://homeassistant.local:1883";
    config.credentials.client_id = device_id;
    config.credentials.username = "mqtt_user";
    config.credentials.authentication.password = "mqtt_user";
    config.session.last_will.topic = availability_topic;
    config.session.last_will.msg = "offline";
    config.session.last_will.qos = 1;
    config.session.last_will.retain = 1;
    config.session.keepalive = 30;
    client = esp_mqtt_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event, nullptr);
    if (err == ESP_OK) err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
    return err;
}
