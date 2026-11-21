#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cJSON.h>
#include <mqtt_client.h>
#include "app_mqtt.h"

static esp_event_handler_t handler;
static std::vector<std::pair<std::string, std::string>> messages;
static std::vector<std::string> subscriptions;
static big_r_animation_t selected = BIG_R_ANIMATION_SOLID;
static bool power = true;
static int commands;
static int client_object;

static const big_r_animation_info_t animations[] = {
    {BIG_R_ANIMATION_RAINBOW_HORIZONTAL, "Rainbow horizontal"},
    {BIG_R_ANIMATION_RAINBOW_VERTICAL, "Rainbow vertical"},
    {BIG_R_ANIMATION_RAINBOW_QUADRANT, "Rainbow quadrant"},
    {BIG_R_ANIMATION_RAINBOW_CENTERED, "Rainbow centered"},
    {BIG_R_ANIMATION_FIRE, "Fire"},
    {BIG_R_ANIMATION_RAIN_SNOW, "Rain/Snow"},
};
extern "C" const big_r_animation_info_t *big_r_led_get_animations(size_t *count)
{ if (count) *count = 6; return animations; }
extern "C" esp_err_t big_r_led_get_state(big_r_led_handle_t, big_r_animation_t *mode, bool *on)
{ *mode = selected; *on = power; return ESP_OK; }
void app_driver_select_mode(intptr_t mode) { ++commands; selected = (big_r_animation_t)mode; }
esp_err_t esp_read_mac(uint8_t *mac, int) { for (int i = 0; i < 6; ++i) mac[i] = i; return ESP_OK; }
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *c)
{
    assert(strcmp(c->broker.address.uri, "mqtt://homeassistant.local:1883") == 0);
    assert(strcmp(c->credentials.client_id, "big_r_000102030405") == 0);
    assert(strcmp(c->session.last_will.msg, "offline") == 0 && c->session.last_will.retain);
    return &client_object;
}
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t, int, esp_event_handler_t h, void *) { handler = h; return ESP_OK; }
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t) { return ESP_OK; }
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t) { return ESP_OK; }
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t, const char *topic, int) { subscriptions.emplace_back(topic); return 1; }
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t, const char *topic, const char *payload, int, int qos, int retain, bool)
{ assert(qos == 1 && retain == 1); messages.emplace_back(topic, payload); return 1; }

static void event(int type, const char *topic = nullptr, const char *data = nullptr, bool retain = false, int total = -1)
{
    esp_mqtt_event_t e = {};
    e.topic = (char *)topic; e.topic_len = topic ? (int)strlen(topic) : 0;
    e.data = (char *)data; e.data_len = data ? (int)strlen(data) : 0;
    e.total_data_len = total < 0 ? e.data_len : total; e.retain = retain;
    handler(nullptr, nullptr, type, &e);
}

int main()
{
    assert(app_mqtt_start(&client_object) == ESP_OK);
    event(MQTT_EVENT_CONNECTED);
    assert(subscriptions.size() == 2 && subscriptions[0] == "big_r_000102030405/animation/set");
    assert(messages.size() == 3);
    assert(messages[0].first == "homeassistant/select/big_r_000102030405/config");
    cJSON *config = cJSON_Parse(messages[0].second.c_str());
    assert(config);
    cJSON *options = cJSON_GetObjectItemCaseSensitive(config, "options");
    assert(cJSON_GetArraySize(options) == 8);
    assert(strcmp(cJSON_GetArrayItem(options, 1)->valuestring, "Rainbow vertical") == 0);
    assert(strcmp(cJSON_GetArrayItem(options, 4)->valuestring, "Fire") == 0);
    assert(strcmp(cJSON_GetArrayItem(options, 5)->valuestring, "Rain/Snow") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(config, "state_topic")->valuestring, "big_r_000102030405/animation/state") == 0);
    assert(strcmp(cJSON_GetArrayItem(options, 6)->valuestring, "Solid") == 0);
    assert(strcmp(cJSON_GetArrayItem(options, 7)->valuestring, "Off") == 0);
    cJSON_Delete(config);
    assert(messages[1].second == "Solid" && messages[2].second == "online");
    const char *topic = "big_r_000102030405/animation/set";
    event(MQTT_EVENT_DATA, topic, "Rainbow vertical");
    assert(commands == 1 && selected == BIG_R_ANIMATION_RAINBOW_VERTICAL);
    event(MQTT_EVENT_DATA, topic, "Off", true);
    event(MQTT_EVENT_DATA, topic, "Off", false, 4);
    event(MQTT_EVENT_DATA, topic, "invalid");
    assert(commands == 1);
    power = false;
    app_mqtt_publish_state();
    assert(messages.back().second == "Off");
    size_t before = messages.size();
    event(MQTT_EVENT_DATA, "homeassistant/status", "online");
    assert(messages.size() == before + 2 && messages.back().second == "Off");
    event(MQTT_EVENT_DISCONNECTED);
    before = messages.size();
    app_mqtt_publish_state();
    assert(messages.size() == before);
    event(MQTT_EVENT_CONNECTED);
    assert(messages.back().second == "online");
}
