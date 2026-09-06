/**
 * TelegramP4 — main application entry point.
 *
 * Phase 0 scope only: bring up logging/NVS and print the startup banner.
 * Wi-Fi, Telegram, camera, etc. are added in later phases — see docs/PHASES.md.
 */
#include <cstdio>
#include "esp_log.h"
#include "nvs_flash.h"
#include "telegramp4_board.h"
#include "telegramp4_wifi.h"
#include "telegramp4_telegram.h"
#include "telegramp4_camera.h"

static const char *TAG = "TAG_SYSTEM";

/**
 * /photo_test (Phase 5) - captures one frame and reports the result. Until the
 * camera driver is verified on real hardware (see telegramp4_camera.h), this
 * will report the honest "camera unavailable" error rather than fake success.
 */
static void handler_photo_test(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_camera_frame_t frame = {0};
    esp_err_t err = telegramp4_camera_capture(&frame);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C Camera unavailable.\nCheck camera connection.");
        return;
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "Frame captured. JPEG size: %u bytes", (unsigned) frame.len);
    telegramp4_telegram_send_message(chat_id, msg);
    telegramp4_camera_release_frame(&frame);
}

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

    esp_err_t camera_ret = telegramp4_camera_init();
    if (camera_ret != ESP_OK) {
        ESP_LOGW(TAG, "Camera not available: %s (device continues without it)", esp_err_to_name(camera_ret));
    }
    telegramp4_telegram_register_command("/photo_test", handler_photo_test);

    esp_err_t telegram_ret = telegramp4_telegram_start();
    if (telegram_ret != ESP_OK) {
        ESP_LOGE(TAG, "Telegram bot failed to start: %s", esp_err_to_name(telegram_ret));
    }

    ESP_LOGI(TAG, "Phase 2 bootstrap complete.");
}
