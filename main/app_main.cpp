/**
 * TelegramP4 — main application entry point.
 *
 * Wires together the components built phase by phase (WiFi, Telegram, camera,
 * storage, ...) — see docs/PHASES.md for what's implemented so far and
 * CLAUDE.md for the live progress checklist. Command handlers that need more
 * than one component (e.g. /photo needs camera + telegram) live here rather
 * than inside either component, to keep components decoupled from each other.
 */
#include <cstdio>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "telegramp4_board.h"
#include "telegramp4_wifi.h"
#include "telegramp4_telegram.h"
#include "telegramp4_camera.h"
#include "telegramp4_storage.h"
#include <dirent.h>
#include <cstring>

static const char *TAG = "TAG_SYSTEM";

/**
 * /photo (Phase 6) - capture then upload via Telegram's sendPhoto. Shares the
 * same camera_capture()/release_frame() pair as /photo_test; once the camera
 * driver is verified on hardware (Phase 5), this starts working with no
 * changes needed here.
 */
static void handler_photo(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_camera_frame_t frame = {0};
    esp_err_t err = telegramp4_camera_capture(&frame);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C Camera unavailable.\nCheck camera connection.");
        return;
    }

    /* Save to SD (Phase 7) before/independent of the upload outcome. */
    telegramp4_storage_status_t storage = telegramp4_storage_get_status();
    if (storage.mounted) {
        char filename[32];
        snprintf(filename, sizeof(filename), "photo_%lld.jpg", (long long) esp_timer_get_time() / 1000000);
        char path[160];
        if (telegramp4_storage_sanitize_path("photos", filename, path, sizeof(path))) {
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(frame.data, 1, frame.len, f);
                fclose(f);
            } else {
                ESP_LOGW(TAG, "Failed to save photo to %s", path);
            }
        }
    }

    err = telegramp4_telegram_send_photo(chat_id, frame.data, frame.len);
    telegramp4_camera_release_frame(&frame);

    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C Telegram upload failed.");
    }
}

/** Formats a byte count as a human-readable "X.Y GB"/"X.Y MB" string. */
static void format_bytes(uint64_t bytes, char *out, size_t out_len)
{
    double gb = (double) bytes / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) {
        snprintf(out, out_len, "%.1f GB", gb);
    } else {
        double mb = (double) bytes / (1024.0 * 1024.0);
        snprintf(out, out_len, "%.1f MB", mb);
    }
}

/* /storage (Phase 7) */
static void handler_storage(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_storage_status_t status = telegramp4_storage_get_status();
    if (!status.mounted) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Storage full.\nSD card not mounted.");
        return;
    }
    char used[16], free_str[16], total[16];
    format_bytes(status.used_bytes, used, sizeof(used));
    format_bytes(status.free_bytes, free_str, sizeof(free_str));
    format_bytes(status.total_bytes, total, sizeof(total));

    char msg[128];
    snprintf(msg, sizeof(msg), "SD Card\n\nUsed: %s\nFree: %s\nTotal: %s", used, free_str, total);
    telegramp4_telegram_send_message(chat_id, msg);
}

/* /files (Phase 7) - per-directory file counts; the full browsable gallery is Phase 8. */
static void handler_files(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_storage_status_t status = telegramp4_storage_get_status();
    if (!status.mounted) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Storage full.\nSD card not mounted.");
        return;
    }
    static const char *dirs[] = {"photos", "videos", "audio", "received", "ai"};
    char msg[256];
    int off = snprintf(msg, sizeof(msg), "Files on SD card:\n");
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && off < (int) sizeof(msg); i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s/%s", TELEGRAMP4_SD_MOUNT_POINT, dirs[i]);
        int count = 0;
        DIR *d = opendir(path);
        if (d) {
            struct dirent *entry;
            while ((entry = readdir(d)) != NULL) {
                if (entry->d_type != DT_DIR) {
                    count++;
                }
            }
            closedir(d);
        }
        off += snprintf(msg + off, sizeof(msg) - off, "%s: %d file(s)\n", dirs[i], count);
    }
    telegramp4_telegram_send_message(chat_id, msg);
}

/**
 * /delete <filename> (Phase 7) - deletes a file from /sdcard/photos/. Filenames
 * are always sanitized before touching the filesystem; a full Yes/Cancel
 * confirmation UI for all destructive actions (this + /reboot) is added
 * project-wide in Phase 22.
 */
static void handler_delete(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        telegramp4_telegram_send_message(chat_id, "Usage: /delete <filename>");
        return;
    }
    char path[160];
    if (!telegramp4_storage_sanitize_path("photos", args, path, sizeof(path))) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    if (remove(path) == 0) {
        telegramp4_telegram_send_message(chat_id, "Deleted.");
    } else {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File not found.");
    }
}

/**
 * /photo_test (Phase 5) - captures one frame and reports the result, without
 * uploading it. Until the camera driver is verified on real hardware (see
 * telegramp4_camera.h), this reports the honest "camera unavailable" error
 * rather than fake success.
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
    telegramp4_telegram_register_command("/photo", handler_photo);

    esp_err_t storage_ret = telegramp4_storage_init();
    if (storage_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available: %s (device continues without it)", esp_err_to_name(storage_ret));
    }
    telegramp4_telegram_register_command("/files", handler_files);
    telegramp4_telegram_register_command("/storage", handler_storage);
    telegramp4_telegram_register_command("/delete", handler_delete);

    esp_err_t telegram_ret = telegramp4_telegram_start();
    if (telegram_ret != ESP_OK) {
        ESP_LOGE(TAG, "Telegram bot failed to start: %s", esp_err_to_name(telegram_ret));
    }

    ESP_LOGI(TAG, "Phase 7 bootstrap complete.");
}
