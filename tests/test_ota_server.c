// Exercise the actual OTA handlers with a host flash/network backend.
#include "../components/lan_ota/lan_ota.c"
#include <openssl/hmac.h>
#include <stdlib.h>
static esp_partition_t current={0x20000,0x1e0000,"ota_0"}, next={0x200000,0x1e0000,"ota_1"};
static esp_app_desc_t app={.project_name="big_r",.version="test"};
static uint8_t stored[32], imported[32], written[20000];
static bool stored_valid, selected, aborted, timer_started, valid, rolled_back, associated=true;
static int begins, end_result, state=ESP_OTA_IMG_VALID, elapsed;
static size_t written_size;
static int64_t now;
void esp_fill_random(void *p,size_t n){static unsigned x;for(size_t i=0;i<n;i++)((uint8_t*)p)[i]=++x;}
int64_t esp_timer_get_time(void){return now;}
void esp_restart(void){}
esp_err_t esp_timer_create(const esp_timer_create_args_t*a,esp_timer_handle_t*t){*t=(void*)1;return 0;}
esp_err_t esp_timer_start_once(esp_timer_handle_t t,uint64_t us){timer_started=true;return 0;}
esp_err_t esp_timer_delete(esp_timer_handle_t t){return 0;}
const esp_app_desc_t *esp_app_get_description(void){return &app;}
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t*p){return &next;}
const esp_partition_t *esp_ota_get_running_partition(void){return &current;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t*p,esp_ota_img_states_t*s){*s=state;return 0;}
esp_err_t esp_ota_begin(const esp_partition_t*p,size_t n,esp_ota_handle_t*h){begins++;written_size=0;*h=1;return 0;}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void*p,size_t n){assert(written_size+n<=sizeof(written));memcpy(written+written_size,p,n);written_size+=n;return 0;}
esp_err_t esp_ota_abort(esp_ota_handle_t h){aborted=true;return 0;}
esp_err_t esp_ota_end(esp_ota_handle_t h){return end_result;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*p){selected=true;return 0;}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void){valid=true;return 0;}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void){rolled_back=true;return 0;}
esp_err_t httpd_start(httpd_handle_t*h,const httpd_config_t*c){*h=(void*)1;return 0;}
esp_err_t httpd_stop(httpd_handle_t h){return 0;}
esp_err_t httpd_register_uri_handler(httpd_handle_t h,const httpd_uri_t*u){return 0;}
esp_err_t httpd_resp_set_status(httpd_req_t*r,const char*s){snprintf(r->status,sizeof(r->status),"%s",s);return 0;}
esp_err_t httpd_resp_set_type(httpd_req_t*r,const char*s){return 0;}
esp_err_t httpd_resp_set_hdr(httpd_req_t*r,const char*k,const char*v){if(!strcmp(k,"Connection"))r->close=true;return 0;}
esp_err_t httpd_resp_send(httpd_req_t*r,const char*b,int n){snprintf(r->response,sizeof(r->response),"%s",b);return 0;}
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t*r,const char*k,char*out,size_t n){const char*v=!strcmp(k,"X-OTA-Nonce")?r->nonce:!strcmp(k,"X-OTA-SHA256")?r->hash:r->auth;if(strlen(v)>=n)return -1;strcpy(out,v);return 0;}
int httpd_req_recv(httpd_req_t*r,char*b,size_t n){if(n>r->available-r->offset)n=r->available-r->offset;if(r->fragment&&n>r->fragment)n=r->fragment;memcpy(b,r->body+r->offset,n);r->offset+=n;return (int)n;}
int xTaskCreate(void(*f)(void*),const char*n,unsigned s,void*a,unsigned p,void*h){return pdPASS;}
void vTaskDelay(unsigned ms){elapsed+=ms/1000;}
void vTaskDelete(void*p){}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char*k){return (esp_netif_t*)1;}
esp_err_t esp_netif_get_ip_info(esp_netif_t*n,esp_netif_ip_info_t*p){p->ip.addr=1;return 0;}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*a){return associated?0:-1;}
esp_err_t nvs_open(const char*n,int m,nvs_handle_t*h){*h=1;return 0;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*p,size_t*n){if(!stored_valid)return ESP_ERR_NVS_NOT_FOUND;memcpy(p,stored,32);*n=32;return 0;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*p,size_t n){assert(n==32);memcpy(stored,p,32);stored_valid=true;return 0;}
esp_err_t nvs_commit(nvs_handle_t h){return 0;}
void nvs_close(nvs_handle_t h){}
int psa_crypto_init(void){return 0;}
void psa_set_key_type(psa_key_attributes_t*a,int x){}
void psa_set_key_usage_flags(psa_key_attributes_t*a,int x){}
void psa_set_key_algorithm(psa_key_attributes_t*a,int x){}
void psa_reset_key_attributes(psa_key_attributes_t*a){}
int psa_import_key(const psa_key_attributes_t*a,const uint8_t*k,size_t n,psa_key_id_t*i){assert(n==32);memcpy(imported,k,n);*i=1;return 0;}
int psa_destroy_key(psa_key_id_t i){memset(imported,0,32);return 0;}
int psa_mac_verify(psa_key_id_t i,int a,const uint8_t*m,size_t n,const uint8_t*s,size_t z){uint8_t d[32];unsigned l;HMAC(EVP_sha256(),imported,32,m,n,d,&l);return z==l&&equal(d,s,z)?0:-1;}
int psa_hash_setup(psa_hash_operation_t*s,int a){s->ctx=EVP_MD_CTX_new();return EVP_DigestInit_ex(s->ctx,EVP_sha256(),NULL)==1?0:-1;}
int psa_hash_update(psa_hash_operation_t*s,const uint8_t*p,size_t n){return EVP_DigestUpdate(s->ctx,p,n)==1?0:-1;}
int psa_hash_finish(psa_hash_operation_t*s,uint8_t*p,size_t n,size_t*l){unsigned z;int r=EVP_DigestFinal_ex(s->ctx,p,&z);*l=z;return r==1?0:-1;}
int psa_hash_abort(psa_hash_operation_t*s){EVP_MD_CTX_free(s->ctx);s->ctx=NULL;return 0;}
static uint8_t image[10000];
static httpd_req_t request(void){
 restarting=selected=aborted=timer_started=false;begins=0;end_result=0;state=ESP_OTA_IMG_VALID;
 httpd_req_t status={0};assert(status_handler(&status)==0);assert(strstr(status.response,"\"slot\":\"ota_0\""));
 httpd_req_t r={.content_len=sizeof(image),.body=image,.available=sizeof(image),.fragment=71};
 hex(nonce,16,r.nonce);uint8_t d[32];unsigned len;EVP_Digest(image,sizeof(image),d,&len,EVP_sha256(),NULL);hex(d,32,r.hash);
 char message[192];int n=snprintf(message,sizeof(message),"BIG-R-OTA-v1\n%s\n%zu\n%s\n",r.nonce,r.content_len,r.hash);
 HMAC(EVP_sha256(),ota_key,32,(uint8_t*)message,n,d,&len);hex(d,32,r.auth);return r;
}
int main(void){
 _Static_assert(sizeof(esp_app_desc_t)==256,"descriptor layout");_Static_assert(PREFIX_SIZE==288,"image prefix layout");
 assert(load_key()==0);uint8_t saved[32];memcpy(saved,ota_key,32);memset(ota_key,0,32);assert(load_key()==0&&equal(saved,ota_key,32));
 image[0]=0xe9;image[12]=5;esp_app_desc_t desc={.magic_word=ESP_APP_DESC_MAGIC_WORD,.project_name="big_r"};memcpy(image+32,&desc,sizeof(desc));
 httpd_req_t r=request();r.auth[0]=r.auth[0]=='0'?'1':'0';assert(upload_handler(&r)==ESP_FAIL&&begins==0&&r.close);
 r=request();now+=CHALLENGE_US+1;assert(upload_handler(&r)==ESP_FAIL&&begins==0);
 r=request();assert(upload_handler(&r)==0&&selected&&timer_started&&written_size==sizeof(image)&&!memcmp(image,written,sizeof(image)));
 restarting=false;r.offset=0;assert(upload_handler(&r)==ESP_FAIL&&begins==1); // replay
 r=request();image[9999]^=1;assert(upload_handler(&r)==ESP_FAIL&&aborted&&!selected);image[9999]^=1;
 r=request();r.available=123;assert(upload_handler(&r)==ESP_FAIL&&begins==0);
 r=request();r.available=400;assert(upload_handler(&r)==ESP_FAIL&&aborted&&!selected);
 r=request();image[80]='x';assert(upload_handler(&r)==ESP_FAIL&&begins==0);image[80]='b';
 r=request();image[12]=4;assert(upload_handler(&r)==ESP_FAIL&&begins==0);image[12]=5;
 r=request();r.content_len=next.size+1;assert(upload_handler(&r)==ESP_FAIL&&begins==0);
 r=request();end_result=-1;assert(upload_handler(&r)==ESP_FAIL&&!selected);
 r=request();state=ESP_OTA_IMG_PENDING_VERIFY;assert(upload_handler(&r)==ESP_FAIL&&begins==0);
 elapsed=0;validate_first_boot(NULL);assert(valid&&!rolled_back&&elapsed==30);
 associated=false;valid=false;elapsed=0;validate_first_boot(NULL);assert(!valid&&rolled_back&&elapsed==90);
 puts("OTA server: authentication, replay, bounds, partial reads, corruption, validation and rollback checks passed");
}
