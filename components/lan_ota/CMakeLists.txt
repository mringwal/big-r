idf_component_register(SRCS "lan_ota.c" INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_http_server app_update esp_app_format esp_timer esp_system
                  esp_netif esp_wifi nvs_flash mbedtls freertos)
