#include "telegramp4_ai.h"
#include "esp_log.h"

static const char *TAG = "TAG_AI";
static bool s_initialized = false;

bool telegramp4_ai_is_enabled(void)
{
#if CONFIG_TELEGRAMP4_AI_ENABLED
    return true;
#else
    return false;
#endif
}

esp_err_t telegramp4_ai_init(void)
{
    if (!telegramp4_ai_is_enabled()) {
        ESP_LOGI(TAG, "AI disabled (TelegramP4 Configuration -> AI -> Enable)");
        return ESP_ERR_NOT_SUPPORTED;
    }
    /*
     * TODO (model/hardware verification required, see header comment and
     * docs/hardware.md): load a real model here (e.g. via ESP-DL) once one
     * has been selected and its PSRAM footprint measured on real hardware.
     */
    ESP_LOGE(TAG, "AI model not yet verified for this board - see docs/hardware.md");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t telegramp4_ai_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t telegramp4_ai_process_image(const uint8_t *image_data, size_t len, telegramp4_ai_result_t *out_result)
{
    (void) image_data; (void) len; (void) out_result;
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_ERR_NOT_SUPPORTED; /* unreachable until init() actually succeeds */
}
