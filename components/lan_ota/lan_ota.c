// Local, manually initiated OTA. HMAC binds each one-use challenge to the exact image.
#include "lan_ota.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define OTA_PORT 8080
#define NONCE_BYTES 16
#define CHALLENGE_US (90LL * 1000000)
#define PREFIX_SIZE (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
static const char *TAG = "lan_ota";
static uint8_t ota_key[32];
static bool key_ready;
static uint8_t nonce[NONCE_BYTES];
static bool nonce_valid;
static int64_t nonce_issued;
static bool restarting;
static httpd_handle_t server;
static esp_timer_handle_t restart_timer;

static void hex(const uint8_t *in, size_t length, char *out)
{
    const char *digits = "0123456789abcdef";
    for (size_t i=0; i<length; ++i) { out[2*i]=digits[in[i]>>4]; out[2*i+1]=digits[in[i]&15]; }
    out[2*length]=0;
}

static bool unhex(const char *in, size_t length, uint8_t *out)
{
    if (strlen(in) != 2*length) return false;
    for (size_t i=0; i<length; ++i) {
        unsigned byte=0;
        for (unsigned j=0; j<2; ++j) {
            char c=in[2*i+j];
            if (c>='0' && c<='9') byte=byte*16+(c-'0');
            else if (c>='a' && c<='f') byte=byte*16+(c-'a'+10);
            else return false;
        }
        out[i]=(uint8_t)byte;
    }
    return true;
}

static bool equal(const uint8_t *a, const uint8_t *b, size_t length)
{
    volatile uint8_t diff=0;
    for (size_t i=0; i<length; ++i) diff |= a[i]^b[i];
    return diff==0;
}

static esp_err_t response(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req,status);
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);
}

static esp_err_t reject(httpd_req_t *req, const char *status, const char *body)
{
    // Never reuse a socket with unread firmware bytes after rejecting a request.
    httpd_resp_set_hdr(req,"Connection","close");
    response(req,status,body);
    return ESP_FAIL;
}

static bool pending_verification(void)
{
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK &&
           state==ESP_OTA_IMG_PENDING_VERIFY;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app=esp_app_get_description();
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    char nonce_text[33], elf_text[65], body[512];
    esp_fill_random(nonce,sizeof(nonce));
    nonce_issued=esp_timer_get_time();
    nonce_valid=true;
    hex(nonce,sizeof(nonce),nonce_text);
    hex(app->app_elf_sha256,sizeof(app->app_elf_sha256),elf_text);
    // Project/version characters originate in the compiled build, not request data.
    // Encode JSON strings explicitly rather than trusting arbitrary build-version text.
    char version[65];
    size_t used=0;
    for (size_t i=0; i<sizeof(app->version) && app->version[i] && used+2<sizeof(version); ++i) {
        unsigned char c=app->version[i];
        if (c=='"' || c=='\\') version[used++]='\\';
        version[used++]=(c>=32 && c<127)?c:'?';
    }
    version[used]=0;
    snprintf(body,sizeof(body),"{\"project\":\"big_r\",\"version\":\"%s\",\"elf_sha256\":\"%s\","
        "\"slot\":\"%s\",\"next_slot\":\"%s\",\"max_image_size\":%lu,\"nonce\":\"%s\",\"rollback_pending\":%s,\"restarting\":%s}",
        version,elf_text,esp_ota_get_running_partition()->label,target?target->label:"",(unsigned long)(target?target->size:0),nonce_text,
        pending_verification()?"true":"false",restarting?"true":"false");
    return response(req,"200 OK",body);
}

static bool authenticate(httpd_req_t *req, uint8_t image_hash[32])
{
    char n[33], hash_text[65], auth_text[65], message[192];
    uint8_t request_nonce[16], signature[32];
    if (!nonce_valid || esp_timer_get_time()-nonce_issued>CHALLENGE_US) return false;
    if (httpd_req_get_hdr_value_str(req,"X-OTA-Nonce",n,sizeof(n))!=ESP_OK ||
        httpd_req_get_hdr_value_str(req,"X-OTA-SHA256",hash_text,sizeof(hash_text))!=ESP_OK ||
        httpd_req_get_hdr_value_str(req,"X-OTA-Auth",auth_text,sizeof(auth_text))!=ESP_OK ||
        !unhex(n,16,request_nonce) || !unhex(hash_text,32,image_hash) || !unhex(auth_text,32,signature) ||
        !equal(request_nonce,nonce,16)) return false;
    int size=snprintf(message,sizeof(message),"BIG-R-OTA-v1\n%s\n%u\n%s\n",n,(unsigned)req->content_len,hash_text);
    if (size<0 || size>=(int)sizeof(message)) return false;
    psa_key_attributes_t attributes=PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes,PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attributes,PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&attributes,PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_key_id_t key_id=0;
    psa_status_t result=psa_import_key(&attributes,ota_key,sizeof(ota_key),&key_id);
    psa_reset_key_attributes(&attributes);
    if (result!=PSA_SUCCESS) return false;
    result=psa_mac_verify(key_id,PSA_ALG_HMAC(PSA_ALG_SHA_256),
                          (const uint8_t *)message,size,signature,sizeof(signature));
    psa_destroy_key(key_id);
    if (result!=PSA_SUCCESS) return false;
    nonce_valid=false; // One valid request consumes the challenge, including a failed upload.
    return true;
}

static bool receive_prefix(httpd_req_t *req, uint8_t *buffer, size_t length)
{
    size_t offset=0;
    while (offset<length) {
        int n=httpd_req_recv(req,(char *)buffer+offset,length-offset);
        if (n<=0) return false;
        offset+=n;
    }
    return true;
}

static void restart(void *unused) { esp_restart(); }

static esp_err_t upload_handler(httpd_req_t *req)
{
    if (restarting || pending_verification()) return reject(req,"409 Conflict","{\"error\":\"restarting or validating first boot\"}");
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if (!target || target->address==esp_ota_get_running_partition()->address)
        return reject(req,"409 Conflict","{\"error\":\"two-slot OTA layout required\"}");
    if (req->content_len<PREFIX_SIZE || req->content_len>target->size)
        return reject(req,"413 Payload Too Large","{\"error\":\"image size outside OTA slot limits\"}");
    uint8_t expected_hash[32];
    if (!authenticate(req,expected_hash)) return reject(req,"403 Forbidden","{\"error\":\"invalid or expired OTA authentication\"}");
    uint8_t buffer[4096];
    if (!receive_prefix(req,buffer,PREFIX_SIZE)) return reject(req,"400 Bad Request","{\"error\":\"incomplete image header\"}");
    esp_image_header_t image;
    esp_app_desc_t description;
    memcpy(&image,buffer,sizeof(image));
    memcpy(&description,buffer+sizeof(image)+sizeof(esp_image_segment_header_t),sizeof(description));
    if (image.magic!=ESP_IMAGE_HEADER_MAGIC || image.chip_id!=ESP_CHIP_ID_ESP32C3 ||
        description.magic_word!=ESP_APP_DESC_MAGIC_WORD ||
        !memchr(description.project_name,0,sizeof(description.project_name)) ||
        strcmp(description.project_name,esp_app_get_description()->project_name)!=0)
        return reject(req,"400 Bad Request","{\"error\":\"not a BIG-R ESP32-C3 application image\"}");
    esp_ota_handle_t ota=0;
    esp_err_t err=esp_ota_begin(target,req->content_len,&ota);
    if (err!=ESP_OK) return reject(req,"500 Internal Server Error","{\"error\":\"cannot begin OTA\"}");
    psa_hash_operation_t sha=PSA_HASH_OPERATION_INIT;
    bool ok=psa_hash_setup(&sha,PSA_ALG_SHA_256)==PSA_SUCCESS && psa_hash_update(&sha,buffer,PREFIX_SIZE)==PSA_SUCCESS;
    if (ok) ok=esp_ota_write(ota,buffer,PREFIX_SIZE)==ESP_OK;
    size_t received=PREFIX_SIZE;
    while (ok && received<req->content_len) {
        size_t wanted=req->content_len-received;
        if (wanted>sizeof(buffer)) wanted=sizeof(buffer);
        int n=httpd_req_recv(req,(char *)buffer,wanted);
        if (n<=0) { ok=false; break; }
        ok=psa_hash_update(&sha,buffer,n)==PSA_SUCCESS && esp_ota_write(ota,buffer,n)==ESP_OK;
        received+=n;
    }
    uint8_t digest[32];
    size_t digest_length=0;
    if (ok) ok=psa_hash_finish(&sha,digest,sizeof(digest),&digest_length)==PSA_SUCCESS &&
               digest_length==32 && equal(digest,expected_hash,32);
    psa_hash_abort(&sha);
    if (!ok) {
        esp_ota_abort(ota);
        ESP_LOGW(TAG,"Upload aborted; running firmware remains selected");
        return reject(req,"400 Bad Request","{\"error\":\"incomplete upload, flash error or SHA256 mismatch\"}");
    }
    // esp_ota_end checks the actual image, including chip/revision/checksum/hash.
    err=esp_ota_end(ota); // End frees the OTA handle even on error.
    if (err!=ESP_OK) return reject(req,"400 Bad Request","{\"error\":\"ESP-IDF rejected the firmware image\"}");
    if (esp_ota_set_boot_partition(target)!=ESP_OK)
        return reject(req,"500 Internal Server Error","{\"error\":\"cannot select new boot slot\"}");
    restarting=true;
    ESP_LOGI(TAG,"Verified %u-byte upload; rebooting into %s",(unsigned)received,target->label);
    esp_err_t sent=response(req,"200 OK","{\"accepted\":true,\"restarting\":true}");
    if (esp_timer_start_once(restart_timer,1000000)!=ESP_OK) esp_restart();
    return sent;
}

static void validate_first_boot(void *unused)
{
    // Startup has already initialized LEDs, Matter, button and the upload server.
    // Keep the image pending for 30s to catch startup crashes, and require a usable IP.
    for (unsigned seconds=0; seconds<90; ++seconds) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_netif_ip_info_t ip={0};
        esp_netif_t *netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        wifi_ap_record_t ap;
        if (seconds>=29 && netif && esp_netif_get_ip_info(netif,&ip)==ESP_OK && ip.ip.addr &&
            esp_wifi_sta_get_ap_info(&ap)==ESP_OK) {
            if (esp_ota_mark_app_valid_cancel_rollback()==ESP_OK) {
                ESP_LOGI(TAG,"First boot validated; rollback cancelled");
                vTaskDelete(NULL);
                return;
            }
            break;
        }
    }
    ESP_LOGE(TAG,"First boot health check failed; rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    esp_restart();
}

esp_err_t lan_ota_export_key(void)
{
    if (!key_ready) return ESP_ERR_INVALID_STATE;
    char text[65];
    hex(ota_key,sizeof(ota_key),text);
    printf("BIG_R_OTA_KEY=%s\n",text);
    return ESP_OK;
}

static esp_err_t load_key(void)
{
    // Keep the original namespace so deployed devices retain their upload key.
    nvs_handle_t handle;
    esp_err_t err=nvs_open("big_r_ota",NVS_READWRITE,&handle);
    if (err!=ESP_OK) return err;
    size_t size=sizeof(ota_key);
    err=nvs_get_blob(handle,"key",ota_key,&size);
    if (err==ESP_ERR_NVS_NOT_FOUND) {
        esp_fill_random(ota_key,sizeof(ota_key));
        err=nvs_set_blob(handle,"key",ota_key,sizeof(ota_key));
        if (err==ESP_OK) err=nvs_commit(handle);
    } else if (err==ESP_OK && size!=sizeof(ota_key)) {
        err=ESP_ERR_INVALID_SIZE;
    }
    nvs_close(handle);
    key_ready=err==ESP_OK;
    return err;
}

esp_err_t lan_ota_start(void)
{
    if (server) return ESP_ERR_INVALID_STATE;
    if (psa_crypto_init()!=PSA_SUCCESS) return ESP_FAIL;
    esp_err_t loaded=load_key();
    if (loaded!=ESP_OK) return loaded;
    esp_timer_create_args_t timer_args={.callback=restart,.name="ota_restart"};
    esp_err_t err=esp_timer_create(&timer_args,&restart_timer);
    if (err!=ESP_OK) return err;
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.server_port=OTA_PORT;
    config.stack_size=10240;
    config.max_uri_handlers=2;
    config.max_open_sockets=2;
    config.lru_purge_enable=true;
    config.recv_wait_timeout=10;
    config.send_wait_timeout=10;
    err=httpd_start(&server,&config);
    if (err!=ESP_OK) { esp_timer_delete(restart_timer); restart_timer=NULL; server=NULL; return err; }
    httpd_uri_t status={.uri="/ota/status",.method=HTTP_GET,.handler=status_handler};
    httpd_uri_t upload={.uri="/ota/upload",.method=HTTP_POST,.handler=upload_handler};
    err=httpd_register_uri_handler(server,&status);
    if (err==ESP_OK) err=httpd_register_uri_handler(server,&upload);
    if (err!=ESP_OK) { httpd_stop(server); server=NULL; esp_timer_delete(restart_timer); restart_timer=NULL; return err; }
    if (pending_verification() && xTaskCreate(validate_first_boot,"ota_health",3072,NULL,3,NULL)!=pdPASS) {
        httpd_stop(server); server=NULL; esp_timer_delete(restart_timer); restart_timer=NULL;
        return ESP_ERR_NO_MEM; // App startup aborts, causing pending-image rollback.
    }
    ESP_LOGI(TAG,"LAN OTA ready on port %d; GET /ota/status, signed POST /ota/upload",OTA_PORT);
    return ESP_OK;
}
