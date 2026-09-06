/**
 * TelegramP4 — main application entry point.
 *
 * Phase 0 scope only: bring up logging/NVS and print the startup banner.
 * Wi-Fi, Telegram, camera, etc. are added in later phases — see docs/PHASES.md.
 */
#include "esp_log.h"
#include "nvs_flash.h"
#include "telegramp4_board.h"
#include "telegramp4_wifi.h"
#include "telegramp4_telegram.h"

static const char *TAG = "TAG_SYSTEM";

extern "C" void app_main(void)
{
    // NVS is required by WiFi and other components that persist state.
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    telegramp4_board_print_banner();

    ESP_ERROR_CHECK(telegramp4_wifi_init());
    if (telegramp4_wifi_wait_connected(CONFIG_TELEGRAMP4_WIFI_CONNECT_TIMEOUT_MS)) {
        char ip[16] = {0};
        telegramp4_wifi_get_ip_str(ip, sizeof(ip));
        ESP_LOGI(TAG, "Boot WiFi connect succeeded, IP: %s", ip);
    } else {
        ESP_LOGW(TAG, "Boot WiFi connect timed out; will keep retrying in the background.");
    }

    esp_err_t telegram_ret = telegramp4_telegram_start();
    if (telegram_ret != ESP_OK) {
        ESP_LOGE(TAG, "Telegram bot failed to start: %s", esp_err_to_name(telegram_ret));
    }

    ESP_LOGI(TAG, "Phase 2 bootstrap complete.");
}
