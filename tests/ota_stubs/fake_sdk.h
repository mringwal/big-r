#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <openssl/evp.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_SIZE 2
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_NVS_NOT_FOUND 4
#define NVS_READWRITE 1
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTPD_RESP_USE_STRLEN -1
#define ESP_OTA_IMG_PENDING_VERIFY 1
#define ESP_OTA_IMG_VALID 2
#define ESP_IMAGE_HEADER_MAGIC 0xe9
#define ESP_CHIP_ID_ESP32C3 5
#define ESP_APP_DESC_MAGIC_WORD 0xabcd5432
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
#define PSA_SUCCESS 0
#define PSA_ALG_SHA_256 1
#define PSA_ALG_HMAC(x) 2
#define PSA_KEY_TYPE_HMAC 3
#define PSA_KEY_USAGE_VERIFY_MESSAGE 4
#define PSA_KEY_ATTRIBUTES_INIT {0}
#define PSA_HASH_OPERATION_INIT {0}
typedef int esp_err_t;
typedef int esp_ota_img_states_t;
typedef int nvs_handle_t;
typedef int esp_ota_handle_t;
typedef void *httpd_handle_t;
typedef void *esp_timer_handle_t;
typedef struct {unsigned address,size; char label[17];} esp_partition_t;
typedef struct {
 uint32_t magic_word,secure_version,reserved[2]; char version[32],project_name[32],time[16],date[16],idf_ver[32];
 uint8_t app_elf_sha256[32],tail[80];
} esp_app_desc_t;
typedef struct {uint8_t magic,segments,mode,speed; uint32_t entry; uint8_t pins[4]; uint16_t chip_id; uint8_t rest[10];} esp_image_header_t;
typedef struct {uint32_t address,length;} esp_image_segment_header_t;
typedef struct httpd_req {
 size_t content_len; const uint8_t *body; size_t available,offset,fragment;
 char nonce[33],hash[65],auth[65],status[64],response[1024]; bool close;
} httpd_req_t;
typedef struct {const char *uri;int method;esp_err_t (*handler)(httpd_req_t *);} httpd_uri_t;
typedef struct {int server_port,stack_size,max_uri_handlers,max_open_sockets,recv_wait_timeout,send_wait_timeout;bool lru_purge_enable;} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
typedef struct {void (*callback)(void *);const char *name;} esp_timer_create_args_t;
typedef struct {struct {uint32_t addr;} ip;} esp_netif_ip_info_t;
typedef struct {} esp_netif_t;
typedef struct {} wifi_ap_record_t;
typedef struct {int unused;} psa_key_attributes_t;
typedef unsigned psa_key_id_t;
typedef int psa_status_t;
typedef struct {EVP_MD_CTX *ctx;} psa_hash_operation_t;
void esp_fill_random(void *,size_t);
int64_t esp_timer_get_time(void);
void esp_restart(void);
esp_err_t esp_timer_create(const esp_timer_create_args_t *,esp_timer_handle_t *);
esp_err_t esp_timer_start_once(esp_timer_handle_t,uint64_t);
esp_err_t esp_timer_delete(esp_timer_handle_t);
const esp_app_desc_t *esp_app_get_description(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *);
const esp_partition_t *esp_ota_get_running_partition(void);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
esp_err_t esp_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
esp_err_t esp_ota_write(esp_ota_handle_t,const void *,size_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
esp_err_t httpd_start(httpd_handle_t *,const httpd_config_t *);
esp_err_t httpd_stop(httpd_handle_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t *);
esp_err_t httpd_resp_set_status(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *,const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *,const char *,const char *);
esp_err_t httpd_resp_send(httpd_req_t *,const char *,int);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *,const char *,char *,size_t);
int httpd_req_recv(httpd_req_t *,char *,size_t);
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,void *);
void vTaskDelay(unsigned);
void vTaskDelete(void *);
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
int psa_crypto_init(void);
void psa_set_key_type(psa_key_attributes_t *,int);
void psa_set_key_usage_flags(psa_key_attributes_t *,int);
void psa_set_key_algorithm(psa_key_attributes_t *,int);
void psa_reset_key_attributes(psa_key_attributes_t *);
int psa_import_key(const psa_key_attributes_t *,const uint8_t *,size_t,psa_key_id_t *);
int psa_destroy_key(psa_key_id_t);
int psa_mac_verify(psa_key_id_t,int,const uint8_t *,size_t,const uint8_t *,size_t);
int psa_hash_setup(psa_hash_operation_t *,int);
int psa_hash_update(psa_hash_operation_t *,const uint8_t *,size_t);
int psa_hash_finish(psa_hash_operation_t *,uint8_t *,size_t,size_t *);
int psa_hash_abort(psa_hash_operation_t *);
