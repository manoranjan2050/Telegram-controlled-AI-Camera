#include "telegramp4_display.h"
#include "esp_log.h"

static const char *TAG = "TAG_DISPLAY";

bool telegramp4_display_is_enabled(void)
{
#if CONFIG_TELEGRAMP4_DISPLAY_ENABLED
    return true;
#else
    return false;
#endif
}

esp_err_t telegramp4_display_init(void)
{
    if (!telegramp4_display_is_enabled()) {
        ESP_LOGI(TAG, "Display disabled (TelegramP4 Configuration -> Display -> Enable)");
        return ESP_ERR_NOT_SUPPORTED;
    }
    /*
     * TODO (hardware verification required, see header comment and
     * docs/hardware.md): bring up the actual MIPI-DSI panel here once its
     * connector/driver is confirmed for this board.
     */
    ESP_LOGE(TAG, "Display hardware not yet verified for this board - see docs/hardware.md");
    return ESP_ERR_NOT_SUPPORTED;
}

void telegramp4_display_update_status(const telegramp4_display_status_t *status)
{
    (void) status;
    if (!telegramp4_display_is_enabled()) {
        return; /* no-op - the rest of the firmware works identically without a display */
    }
    /* TODO: draw once telegramp4_display_init() actually succeeds. */
}
