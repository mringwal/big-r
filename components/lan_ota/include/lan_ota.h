#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
// Call after the application/LED/Matter initialization has succeeded.
// Serves status and authenticated raw-image uploads on the local Wi-Fi interface.
esp_err_t lan_ota_start(void);
// Export over the physical UART console only, for one-time laptop provisioning.
esp_err_t lan_ota_export_key(void);
#ifdef __cplusplus
}
#endif
