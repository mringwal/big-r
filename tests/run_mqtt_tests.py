#!/usr/bin/env python3
"""Exercise the real MQTT event handler with fake ESP-MQTT and Matter backends."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = {
    "esp_err.h": '''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
''',
    "esp_log.h": '''#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
''',
    "esp_mac.h": '''#pragma once
#include <stdint.h>
#include <esp_err.h>
#define ESP_MAC_WIFI_STA 0
esp_err_t esp_read_mac(uint8_t *, int);
''',
    "platform/CHIPDeviceLayer.h": '''#pragma once
#include <stdint.h>
struct CHIP_ERROR { int value; int Format() const { return value; } };
inline bool operator!=(CHIP_ERROR e, int n) { return e.value != n; }
#define CHIP_NO_ERROR 0
#define CHIP_ERROR_FORMAT "d"
namespace chip { namespace DeviceLayer {
struct PlatformManager { CHIP_ERROR ScheduleWork(void (*fn)(intptr_t), intptr_t arg) { fn(arg); return {0}; } };
inline PlatformManager &PlatformMgr() { static PlatformManager p; return p; }
} }
''',
    "mqtt_client.h": '''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
typedef void *esp_mqtt_client_handle_t;
typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
enum { MQTT_EVENT_ANY, MQTT_EVENT_CONNECTED, MQTT_EVENT_DISCONNECTED, MQTT_EVENT_DATA };
typedef struct { char *data; int data_len, total_data_len, current_data_offset; char *topic; int topic_len; bool retain; } esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
typedef struct {
    struct { struct { const char *uri; } address; } broker;
    struct { const char *client_id; const char *username; struct { const char *password; } authentication; } credentials;
    struct { struct { const char *topic, *msg; int qos, retain; } last_will; int keepalive; } session;
} esp_mqtt_client_config_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t, int, esp_event_handler_t, void *);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t, const char *, int);
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t, const char *, const char *, int, int, int, bool);
''',
}

with tempfile.TemporaryDirectory(prefix="big-r-mqtt-test-") as temporary:
    tmp = Path(temporary)
    for name, content in headers.items():
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    cjson = root / "managed_components/espressif__cjson/cJSON"
    includes = ["-I" + str(p) for p in (tmp, root / "main", root / "components/big_r_led/include", cjson)]
    common = ["-fsanitize=address,undefined", "-Wno-deprecated-declarations", "-g", *includes]
    for source, output in ((cjson / "cJSON.c", "json.o"), (root / "main/mqtt_select.c", "select.o")):
        subprocess.run(["/usr/bin/cc", "-std=c11", *common, "-c", str(source), "-o", str(tmp / output)], check=True)
    subprocess.run(["/usr/bin/c++", "-std=c++17", *common, str(root / "tests/test_mqtt.cpp"),
                    str(root / "main/app_mqtt.cpp"), str(tmp / "json.o"), str(tmp / "select.o"),
                    "-o", str(tmp / "test")], check=True)
    subprocess.run([str(tmp / "test")], check=True)
    print("PASS: MQTT discovery, subscriptions, command validation, state, reconnect and HA birth")
