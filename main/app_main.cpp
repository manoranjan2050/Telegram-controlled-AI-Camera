/**
 * TelegramP4 — main application entry point.
 *
 * Phase 0 scope only: bring up logging/NVS and print the startup banner.
 * Wi-Fi, Telegram, camera, etc. are added in later phases — see docs/PHASES.md.
 */
#include "esp_log.h"
#include "nvs_flash.h"
#include "telegramp4_board.h"

static const char *TAG = "TAG_SYSTEM";

extern "C" void app_main(void)
{
    // NVS is required by Wi-Fi (Phase 1) and other components that persist state;
    // initializing it here in Phase 0 avoids re-plumbing bootstrap later.
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    telegramp4_board_print_banner();

    ESP_LOGI(TAG, "Phase 0 bootstrap complete.");
}
