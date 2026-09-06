#include "telegramp4_ota.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "TAG_SYSTEM"; /* OTA logs under the system tag - see docs/PHASES.md logging tags */

esp_err_t telegramp4_ota_update_from_url(const char *url)
{
#if !CONFIG_TELEGRAMP4_OTA_ENABLED
    (void) url;
    ESP_LOGW(TAG, "OTA disabled (TelegramP4 Configuration -> OTA -> Enable)");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_LOGI(TAG, "Starting OTA update");

    esp_http_client_config_t http_config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach, /* TLS validated, never disabled */
        .timeout_ms = 30 * 1000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t err = esp_https_ota(&ota_config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA succeeded, rebooting into new firmware");
        esp_restart(); /* does not return */
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
    return err;
#endif
}
