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
#include "esp_idf_version.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "telegramp4_board.h"
#include "telegramp4_wifi.h"
#include "telegramp4_telegram.h"
#include "telegramp4_camera.h"
#include "telegramp4_storage.h"
#include "telegramp4_video.h"
#include "telegramp4_audio.h"
#include "telegramp4_stt.h"
#include "telegramp4_ai.h"
#include "telegramp4_motion.h"
#include "telegramp4_gpio.h"
#include "telegramp4_display.h"
#include "telegramp4_ota.h"
#include "esp_heap_caps.h"
#include "telegramp4_security.h"
#include <ctime>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdlib>
#include <cctype>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include "cJSON.h"

static const char *TAG = "TAG_SYSTEM";
static int64_t s_last_ai_inference_ms = -1; /* -1 = no AI run yet; set by handler_ai(), read by /diagnostics */

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

/* --- Confirmations for destructive actions (Phase 22) --- */

static void send_confirmation(int64_t chat_id, const char *yes_label, const char *yes_callback,
                                const char *no_callback)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *keyboard = cJSON_AddArrayToObject(root, "inline_keyboard");
    cJSON *row = cJSON_CreateArray();
    cJSON *yes = cJSON_CreateObject();
    cJSON_AddStringToObject(yes, "text", yes_label);
    cJSON_AddStringToObject(yes, "callback_data", yes_callback);
    cJSON *no = cJSON_CreateObject();
    cJSON_AddStringToObject(no, "text", "Cancel");
    cJSON_AddStringToObject(no, "callback_data", no_callback);
    cJSON_AddItemToArray(row, yes);
    cJSON_AddItemToArray(row, no);
    cJSON_AddItemToArray(keyboard, row);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    telegramp4_telegram_send_with_keyboard(chat_id, "Are you sure?", json);
    free(json);
}

static void handler_delete_confirm(int64_t chat_id, const char *args)
{
    const char *colon = strchr(args, ':');
    char subdir[16] = {0};
    if (!colon || (size_t) (colon - args) >= sizeof(subdir)) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid request.");
        return;
    }
    memcpy(subdir, args, colon - args);
    const char *filename = colon + 1;

    char path[160];
    if (!telegramp4_storage_sanitize_path(subdir, filename, path, sizeof(path))) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    if (remove(path) == 0) {
        telegramp4_telegram_send_message(chat_id, "Deleted.");
    } else {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File not found.");
    }
}

static void handler_cancel(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_message(chat_id, "Cancelled.");
}

static void handler_reboot(int64_t chat_id, const char *args)
{
    (void) args;
    send_confirmation(chat_id, "Yes, reboot", "/reboot_confirm", "/cancel");
}

static void handler_reboot_confirm(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_message(chat_id, "Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500)); /* let the HTTP request above actually complete first */
    esp_restart();
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
 * Shared by /delete and the gallery's [Delete] button - both ask for
 * confirmation (Phase 22) before calling handler_delete_confirm(), which
 * does the actual sanitize+remove. `subdir` is always a fixed string literal
 * from the caller, never user input.
 */
static void request_delete_confirmation(int64_t chat_id, const char *subdir, const char *filename)
{
    char callback[192];
    snprintf(callback, sizeof(callback), "/delete_confirm %s:%s", subdir, filename);
    send_confirmation(chat_id, "Yes, delete", callback, "/cancel");
}

/* /delete <filename> (Phase 7, confirmation added Phase 22) */
static void handler_delete(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        telegramp4_telegram_send_message(chat_id, "Usage: /delete <filename>");
        return;
    }
    request_delete_confirmation(chat_id, "photos", args);
}

/* --- Photo gallery (Phase 8) --- */

#define GALLERY_MAX_LISTED 8

/** Lists up to `max_count` photo filenames, newest first (best-effort: sorted
 * by filename descending, which matches chronological order for our
 * "photo_<unix_seconds>.jpg" naming as long as the digit count doesn't change
 * mid-list). */
static int list_recent_photos(char names[][40], int max_count)
{
    DIR *d = opendir(TELEGRAMP4_SD_MOUNT_POINT "/photos");
    if (!d) {
        return 0;
    }
    std::vector<std::string> all;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_DIR) {
            all.push_back(entry->d_name);
        }
    }
    closedir(d);

    std::sort(all.begin(), all.end(), std::greater<std::string>());

    int count = (int) all.size() < max_count ? (int) all.size() : max_count;
    for (int i = 0; i < count; i++) {
        strncpy(names[i], all[i].c_str(), 39);
        names[i][39] = '\0';
    }
    return count;
}

static char *build_gallery_keyboard_json(char names[][40], int count)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *keyboard = cJSON_AddArrayToObject(root, "inline_keyboard");
    for (int i = 0; i < count; i++) {
        cJSON *row = cJSON_CreateArray();
        struct { const char *label; const char *prefix; } buttons[] = {
            {"View", "/photo_view "}, {"Download", "/photo_dl "}, {"\xF0\x9F\x97\x91 Delete", "/photo_del "},
        };
        for (auto &b : buttons) {
            char callback[64];
            snprintf(callback, sizeof(callback), "%s%s", b.prefix, names[i]);
            cJSON *btn = cJSON_CreateObject();
            cJSON_AddStringToObject(btn, "text", b.label);
            cJSON_AddStringToObject(btn, "callback_data", callback);
            cJSON_AddItemToArray(row, btn);
        }
        cJSON_AddItemToArray(keyboard, row);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static bool read_file_into_buffer(const char *path, uint8_t **out_data, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return false;
    }
    uint8_t *buf = (uint8_t *) malloc((size_t) size);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t read = fread(buf, 1, (size_t) size, f);
    fclose(f);
    if (read != (size_t) size) {
        free(buf);
        return false;
    }
    *out_data = buf;
    *out_len = (size_t) size;
    return true;
}

/* /photos (Phase 8) - lists recent photos with [View][Download][Delete] buttons. */
static void handler_photos(int64_t chat_id, const char *args)
{
    (void) args;
    char names[GALLERY_MAX_LISTED][40];
    int count = list_recent_photos(names, GALLERY_MAX_LISTED);
    if (count == 0) {
        telegramp4_telegram_send_message(chat_id, "No photos on SD card yet. Try /photo first.");
        return;
    }
    char *keyboard = build_gallery_keyboard_json(names, count);
    if (!keyboard) {
        return;
    }
    char text[256];
    int off = snprintf(text, sizeof(text), "Recent photos (%d):\n", count);
    for (int i = 0; i < count && off < (int) sizeof(text); i++) {
        off += snprintf(text + off, sizeof(text) - off, "%s\n", names[i]);
    }
    telegramp4_telegram_send_with_keyboard(chat_id, text, keyboard);
    free(keyboard);
}

static void handler_photo_view(int64_t chat_id, const char *args)
{
    char path[160];
    if (args[0] == '\0' || !telegramp4_storage_sanitize_path("photos", args, path, sizeof(path))) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    uint8_t *data = NULL;
    size_t len = 0;
    if (!read_file_into_buffer(path, &data, &len)) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File not found.");
        return;
    }
    telegramp4_telegram_send_photo(chat_id, data, len);
    free(data);
}

static void handler_photo_download(int64_t chat_id, const char *args)
{
    char path[160];
    if (args[0] == '\0' || !telegramp4_storage_sanitize_path("photos", args, path, sizeof(path))) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    uint8_t *data = NULL;
    size_t len = 0;
    if (!read_file_into_buffer(path, &data, &len)) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File not found.");
        return;
    }
    telegramp4_telegram_send_document(chat_id, data, len, args);
    free(data);
}

static void handler_photo_delete_cb(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        return;
    }
    request_delete_confirmation(chat_id, "photos", args);
}

/**
 * /photo_test (Phase 5) - captures one frame and reports the result, without
 * uploading it. Until the camera driver is verified on real hardware (see
 * telegramp4_camera.h), this reports the honest "camera unavailable" error
 * rather than fake success.
 */
/* --- Video (Phase 9) --- */

struct video_task_args_t {
    int64_t chat_id;
    uint32_t duration_s;
};

/**
 * Runs on its own task, never on the Telegram poll task, so a (currently
 * stubbed, eventually multi-second) recording never blocks command handling
 * for other chats - see docs/architecture.md task model.
 */
static void video_record_task(void *arg)
{
    video_task_args_t *a = (video_task_args_t *) arg;

    char msg[64];
    snprintf(msg, sizeof(msg), "\xF0\x9F\x8E\xA5 Recording for %u seconds...", (unsigned) a->duration_s);
    telegramp4_telegram_send_message(a->chat_id, msg);

    telegramp4_video_result_t result = {0};
    esp_err_t err = telegramp4_video_record(a->duration_s, &result);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(a->chat_id,
            "\xE2\x9D\x8C Camera unavailable.\nCheck camera connection.");
    } else {
        uint8_t *data = NULL;
        size_t len = 0;
        if (read_file_into_buffer(result.path, &data, &len)) {
            telegramp4_telegram_send_message(a->chat_id, "Uploading...");
            telegramp4_telegram_send_document(a->chat_id, data, len, "video.mp4");
            free(data);
        }
    }

    free(a);
    vTaskDelete(NULL);
}

/* /video [seconds] (Phase 9) */
static void handler_video(int64_t chat_id, const char *args)
{
    long seconds = (args[0] != '\0') ? atol(args) : CONFIG_TELEGRAMP4_VIDEO_DEFAULT_DURATION_S;
    if (seconds < CONFIG_TELEGRAMP4_VIDEO_MIN_DURATION_S) {
        seconds = CONFIG_TELEGRAMP4_VIDEO_MIN_DURATION_S;
    }
    if (seconds > CONFIG_TELEGRAMP4_VIDEO_MAX_DURATION_S) {
        seconds = CONFIG_TELEGRAMP4_VIDEO_MAX_DURATION_S;
    }

    auto *task_args = (video_task_args_t *) malloc(sizeof(video_task_args_t));
    task_args->chat_id = chat_id;
    task_args->duration_s = (uint32_t) seconds;

    /* 16KB: this task makes HTTPS calls (send_message/send_document), which
     * caused a real stack-overflow crash at 8KB on hardware - see the note on
     * telegram_poll_task's stack size in telegramp4_telegram.c. */
    if (xTaskCreate(video_record_task, "video_record", 16384, task_args, 5, NULL) != pdPASS) {
        free(task_args);
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Failed to start recording task.");
    }
}

/* --- Receive photo from Telegram (Phase 10) --- */

static char s_last_received_filename[40] = {0};
static size_t s_last_received_size = 0;

static void on_photo_received(int64_t chat_id, const char *file_id, size_t declared_size, uint32_t duration_s)
{
    (void) duration_s;
    size_t max_bytes = (size_t) CONFIG_TELEGRAMP4_STORAGE_MAX_DOWNLOAD_SIZE_KB * 1024;
    if (declared_size > 0 && declared_size > max_bytes) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File too large.");
        return;
    }

    telegramp4_storage_status_t storage = telegramp4_storage_get_status();
    if (!storage.mounted) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Storage full.\nSD card not mounted.");
        return;
    }

    uint8_t *data = NULL;
    size_t len = 0;
    esp_err_t err = telegramp4_telegram_download_file(file_id, max_bytes, &data, &len);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Download failed or file too large.");
        return;
    }

    char filename[40];
    snprintf(filename, sizeof(filename), "received_%lld.jpg", (long long) esp_timer_get_time() / 1000000);
    char path[160];
    if (!telegramp4_storage_sanitize_path("received", filename, path, sizeof(path))) {
        free(data);
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    }
    free(data);

    strncpy(s_last_received_filename, filename, sizeof(s_last_received_filename) - 1);
    s_last_received_size = len;

    char size_str[16];
    format_bytes(len, size_str, sizeof(size_str));
    char msg[128];
    snprintf(msg, sizeof(msg), "\xF0\x9F\x93\xA5 Image received.\n\nFile:\n%s\n\nSize:\n%s", filename, size_str);
    telegramp4_telegram_send_message(chat_id, msg);

    /* Phase 16: offer what to do next with the just-saved image. */
    cJSON *root = cJSON_CreateObject();
    cJSON *keyboard = cJSON_AddArrayToObject(root, "inline_keyboard");
    struct { const char *label; const char *prefix; } buttons[] = {
        {"\xF0\x9F\xA4\x96 Detect Objects", "/received_ai "},
        {"\xF0\x9F\x92\xBE Save", "/received_save "},
        {"\xF0\x9F\x97\x91 Delete", "/received_delete "},
    };
    for (auto &b : buttons) {
        cJSON *row = cJSON_CreateArray();
        char callback[64];
        snprintf(callback, sizeof(callback), "%s%s", b.prefix, filename);
        cJSON *btn = cJSON_CreateObject();
        cJSON_AddStringToObject(btn, "text", b.label);
        cJSON_AddStringToObject(btn, "callback_data", callback);
        cJSON_AddItemToArray(row, btn);
        cJSON_AddItemToArray(keyboard, row);
    }
    char *keyboard_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    telegramp4_telegram_send_with_keyboard(chat_id, "What would you like to do?", keyboard_json);
    free(keyboard_json);
}

/* --- AI on Telegram photos (Phase 16) --- */

static void handler_received_ai(int64_t chat_id, const char *args)
{
    char path[160];
    if (args[0] == '\0' || !telegramp4_storage_sanitize_path("received", args, path, sizeof(path))) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    if (!telegramp4_ai_is_enabled()) {
        telegramp4_telegram_send_message(chat_id,
            "AI is disabled. Enable it in idf.py menuconfig -> TelegramP4 Configuration -> AI.");
        return;
    }
    uint8_t *data = NULL;
    size_t len = 0;
    if (!read_file_into_buffer(path, &data, &len)) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File not found.");
        return;
    }
    telegramp4_ai_result_t result = {0};
    esp_err_t err = telegramp4_ai_process_image(data, len, &result);
    free(data);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C AI inference failed: model not yet verified on this hardware.");
        return;
    }
    char msg[320];
    int off = snprintf(msg, sizeof(msg), "\xF0\x9F\xA4\x96 AI Detection\n\nDetected:\n");
    for (int i = 0; i < result.count && off < (int) sizeof(msg); i++) {
        off += snprintf(msg + off, sizeof(msg) - off, "%s - %u%%\n",
                          result.detections[i].label, (unsigned) result.detections[i].confidence_pct);
    }
    off += snprintf(msg + off, sizeof(msg) - off, "\nInference:\n%u ms", (unsigned) result.inference_time_ms);
    telegramp4_telegram_send_message(chat_id, msg);
}

static void handler_received_save(int64_t chat_id, const char *args)
{
    (void) args;
    /* Already saved on receipt (Phase 10) - this just confirms the choice. */
    telegramp4_telegram_send_message(chat_id, "Saved.");
}

static void handler_received_delete(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    request_delete_confirmation(chat_id, "received", args);
}

/**
 * Phase 13 - maps a recognized phrase to a registered command. Deliberately
 * simple keyword matching; the point isn't NLP sophistication, it's that a
 * match is dispatched through telegramp4_telegram_dispatch() - the exact same
 * path as a typed command - never a separate execution mechanism.
 */
static const char *parse_voice_command(const char *text)
{
    char lower[128];
    size_t i = 0;
    for (; text[i] != '\0' && i < sizeof(lower) - 1; i++) {
        lower[i] = (char) tolower((unsigned char) text[i]);
    }
    lower[i] = '\0';

    if (strstr(lower, "photo") || strstr(lower, "picture")) return "/photo";
    if (strstr(lower, "video")) return "/video";
    if (strstr(lower, "status")) return "/status";
    if (strstr(lower, "record")) return "/record";
    if (strstr(lower, "storage") || strstr(lower, "sd card")) return "/storage";
    return NULL;
}

/* --- Receive voice from Telegram (Phase 12) ---
 * Telegram voice notes are OGG/Opus - saved as-is, no decoding/transcoding.
 * Phase 13 adds optional speech-to-text on top (TELEGRAMP4_STT_ENABLED),
 * routing recognized phrases through the same command dispatch as everything
 * else - see parse_voice_command() above and telegramp4_telegram_dispatch(). */
static void on_voice_received(int64_t chat_id, const char *file_id, size_t declared_size, uint32_t duration_s)
{
    size_t max_bytes = (size_t) CONFIG_TELEGRAMP4_STORAGE_MAX_DOWNLOAD_SIZE_KB * 1024;
    if (declared_size > 0 && declared_size > max_bytes) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C File too large.");
        return;
    }
    telegramp4_storage_status_t storage = telegramp4_storage_get_status();
    if (!storage.mounted) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Storage full.\nSD card not mounted.");
        return;
    }

    uint8_t *data = NULL;
    size_t len = 0;
    if (telegramp4_telegram_download_file(file_id, max_bytes, &data, &len) != ESP_OK) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Download failed or file too large.");
        return;
    }

    char filename[40];
    snprintf(filename, sizeof(filename), "voice_%lld.ogg", (long long) esp_timer_get_time() / 1000000);
    char path[160];
    if (!telegramp4_storage_sanitize_path("received", filename, path, sizeof(path))) {
        free(data);
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Invalid filename.");
        return;
    }
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    }

    strncpy(s_last_received_filename, filename, sizeof(s_last_received_filename) - 1);
    s_last_received_size = len;

    char msg[128];
    snprintf(msg, sizeof(msg),
        "\xF0\x9F\x8E\x99 Voice message received.\n\nDuration: %u sec\nSaved:\nreceived/%s",
        (unsigned) duration_s, filename);
    telegramp4_telegram_send_message(chat_id, msg);

    /* Phase 13: optional voice-command processing, using the bytes already in
     * memory rather than re-reading the file we just wrote. */
    char stt_text[256];
    esp_err_t stt_err = telegramp4_stt_transcribe(data, len, stt_text, sizeof(stt_text));
    free(data);

    if (stt_err == ESP_OK) {
        const char *cmd = parse_voice_command(stt_text);
        char voice_msg[384];
        if (cmd) {
            snprintf(voice_msg, sizeof(voice_msg),
                "Speech recognized:\n\"%s\"\n\nCommand:\n%s\n\nExecuting...", stt_text, cmd + 1);
            telegramp4_telegram_send_message(chat_id, voice_msg);
            telegramp4_telegram_dispatch(chat_id, cmd);
        } else {
            snprintf(voice_msg, sizeof(voice_msg),
                "Speech recognized:\n\"%s\"\n\nNo matching command found.", stt_text);
            telegramp4_telegram_send_message(chat_id, voice_msg);
        }
    }
    /* stt_err != ESP_OK (disabled, no API key, or request failure) is not an
     * error to the user here - Phase 12's save-and-acknowledge behavior above
     * already completed successfully; voice commands are a bonus on top. */
}

/* /ai (Phase 15) - capture -> AI inference -> photo + text result. */
static void handler_ai(int64_t chat_id, const char *args)
{
    (void) args;
    if (!telegramp4_ai_is_enabled()) {
        telegramp4_telegram_send_message(chat_id,
            "AI is disabled. Enable it in idf.py menuconfig -> TelegramP4 Configuration -> AI.");
        return;
    }

    telegramp4_camera_frame_t frame = {0};
    if (telegramp4_camera_capture(&frame) != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C Camera unavailable.\nCheck camera connection.");
        return;
    }

    telegramp4_ai_result_t result = {0};
    esp_err_t err = telegramp4_ai_process_image(frame.data, frame.len, &result);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id,
            "\xE2\x9D\x8C AI inference failed: model not yet verified on this hardware.");
        telegramp4_camera_release_frame(&frame);
        return;
    }

    telegramp4_telegram_send_photo(chat_id, frame.data, frame.len);
    telegramp4_camera_release_frame(&frame);
    s_last_ai_inference_ms = result.inference_time_ms;

    char msg[320];
    int off = snprintf(msg, sizeof(msg), "\xF0\x9F\xA4\x96 AI Detection\n\nDetected:\n");
    for (int i = 0; i < result.count && off < (int) sizeof(msg); i++) {
        off += snprintf(msg + off, sizeof(msg) - off, "%s - %u%%\n",
                          result.detections[i].label, (unsigned) result.detections[i].confidence_pct);
    }
    off += snprintf(msg + off, sizeof(msg) - off, "\nInference:\n%u ms", (unsigned) result.inference_time_ms);
    telegramp4_telegram_send_message(chat_id, msg);
}

/* --- Motion detection + AI alert (Phase 17/18) --- */

static int64_t s_last_motion_alert_s = -1000000; /* far in the past so the first trigger always fires */

static void broadcast_to_all_authorized(const char *text, const uint8_t *photo_data, size_t photo_len)
{
    int64_t ids[16];
    int n = telegramp4_security_get_allowed_ids(ids, 16);
    for (int i = 0; i < n; i++) {
        telegramp4_telegram_send_message(ids[i], text);
        if (photo_data) {
            telegramp4_telegram_send_photo(ids[i], photo_data, photo_len);
        }
    }
}

/**
 * Called from telegramp4_motion's own task (never ISR context) when armed and
 * motion fires. Cooldown prevents Telegram spam from repeated triggers.
 */
static void on_motion_detected(void)
{
    int64_t now_s = esp_timer_get_time() / 1000000;
    if (now_s - s_last_motion_alert_s < CONFIG_TELEGRAMP4_MOTION_ALERT_COOLDOWN_S) {
        ESP_LOGI(TAG, "Motion detected but within cooldown, ignoring");
        return;
    }

    telegramp4_camera_frame_t frame = {0};
    if (telegramp4_camera_capture(&frame) != ESP_OK) {
        ESP_LOGW(TAG, "Motion detected but camera unavailable, cannot verify/alert");
        return;
    }

    /* If AI is enabled and actually runs, filter for a "person" label before
     * alerting. If AI is disabled, or enabled but not yet working (Phase 14's
     * stub), fall back to alerting on raw motion - a security camera that
     * stays silent because its AI is unverified defeats the point, and we
     * say so honestly in the message rather than fabricating a confidence
     * number. */
    telegramp4_ai_result_t result = {0};
    bool ai_ran = telegramp4_ai_is_enabled() && telegramp4_ai_process_image(frame.data, frame.len, &result) == ESP_OK;
    bool person_found = !ai_ran; /* unfiltered alert if AI didn't actually run */
    int person_confidence = -1;
    if (ai_ran) {
        person_found = false;
        for (int i = 0; i < result.count; i++) {
            if (strstr(result.detections[i].label, "erson") != NULL) { /* "Person"/"person" */
                person_found = true;
                person_confidence = result.detections[i].confidence_pct;
                break;
            }
        }
    }

    if (!person_found) {
        telegramp4_camera_release_frame(&frame);
        return; /* AI ran and found no person - don't alert */
    }

    s_last_motion_alert_s = now_s;
    int64_t uptime_s = now_s;
    char msg[192];
    if (person_confidence >= 0) {
        snprintf(msg, sizeof(msg),
            "\xF0\x9F\x9A\xA8 Person detected\n\nConfidence: %d%%\nUptime at detection: %02d:%02d:%02d",
            person_confidence, (int) (uptime_s / 3600), (int) ((uptime_s / 60) % 60), (int) (uptime_s % 60));
    } else {
        snprintf(msg, sizeof(msg),
            "\xF0\x9F\x9A\xA8 Motion detected (AI filtering unavailable)\n\nUptime at detection: %02d:%02d:%02d",
            (int) (uptime_s / 3600), (int) ((uptime_s / 60) % 60), (int) (uptime_s % 60));
    }
    broadcast_to_all_authorized(msg, frame.data, frame.len);
    telegramp4_camera_release_frame(&frame);
}

static void handler_arm(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_motion_arm();
    telegramp4_telegram_send_message(chat_id, "Motion detection: ARMED");
}

static void handler_disarm(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_motion_disarm();
    telegramp4_telegram_send_message(chat_id, "Motion detection: DISARMED");
}

static void handler_motion_status(int64_t chat_id, const char *args)
{
    (void) args;
    char msg[96];
    snprintf(msg, sizeof(msg), "Motion detection: %s\nAI filtering: %s",
              telegramp4_motion_is_armed() ? "ARMED" : "DISARMED",
              telegramp4_ai_is_enabled() ? "ON" : "OFF");
    telegramp4_telegram_send_message(chat_id, msg);
}

/* --- GPIO control (Phase 19) --- */

static void send_gpio_menu(int64_t chat_id)
{
    int pins[TELEGRAMP4_GPIO_MAX_WHITELISTED];
    int n = telegramp4_gpio_get_whitelist(pins, TELEGRAMP4_GPIO_MAX_WHITELISTED);
    if (n == 0) {
        telegramp4_telegram_send_message(chat_id,
            "No GPIOs configured. Set TELEGRAMP4_GPIO_WHITELIST in idf.py menuconfig.");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *keyboard = cJSON_AddArrayToObject(root, "inline_keyboard");
    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_CreateArray();
        char label[24];
        snprintf(label, sizeof(label), "GPIO %d [%s]", pins[i], telegramp4_gpio_get(pins[i]) ? "ON" : "OFF");
        char callback[32];
        snprintf(callback, sizeof(callback), "/gpio_toggle %d", pins[i]);
        cJSON *btn = cJSON_CreateObject();
        cJSON_AddStringToObject(btn, "text", label);
        cJSON_AddStringToObject(btn, "callback_data", callback);
        cJSON_AddItemToArray(row, btn);
        cJSON_AddItemToArray(keyboard, row);
    }
    char *keyboard_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    telegramp4_telegram_send_with_keyboard(chat_id, "\xF0\x9F\x94\x8C GPIO Control", keyboard_json);
    free(keyboard_json);
}

/* /gpio, /gpio <pin> on|off */
static void handler_gpio(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        send_gpio_menu(chat_id);
        return;
    }
    int pin = 0;
    char state[8] = {0};
    if (sscanf(args, "%d %7s", &pin, state) != 2) {
        telegramp4_telegram_send_message(chat_id, "Usage: /gpio <pin> on|off");
        return;
    }
    if (!telegramp4_gpio_is_whitelisted(pin)) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C GPIO not whitelisted.");
        return;
    }
    bool on = (strcasecmp(state, "on") == 0);
    telegramp4_gpio_set(pin, on);
    char msg[32];
    snprintf(msg, sizeof(msg), "GPIO %d: %s", pin, on ? "ON" : "OFF");
    telegramp4_telegram_send_message(chat_id, msg);
}

static void handler_gpio_toggle(int64_t chat_id, const char *args)
{
    int pin = atoi(args);
    if (!telegramp4_gpio_is_whitelisted(pin)) {
        return;
    }
    telegramp4_gpio_set(pin, !telegramp4_gpio_get(pin));
    send_gpio_menu(chat_id);
}

/* --- Full system status / diagnostics (Phase 22) --- */

static void handler_full_status(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_wifi_state_t wifi_state = telegramp4_wifi_get_state();
    char ip[16] = "N/A";
    int8_t rssi = 0;
    if (wifi_state == TELEGRAMP4_WIFI_STATE_CONNECTED) {
        telegramp4_wifi_get_ip_str(ip, sizeof(ip));
        rssi = telegramp4_wifi_get_rssi();
    }
    telegramp4_camera_status_t cam = telegramp4_camera_get_status();
    telegramp4_storage_status_t storage = telegramp4_storage_get_status();
    char free_str[16] = "N/A";
    if (storage.mounted) {
        format_bytes(storage.free_bytes, free_str, sizeof(free_str));
    }
    int64_t uptime_s = esp_timer_get_time() / 1000000;

    char msg[512];
    snprintf(msg, sizeof(msg),
        "\xF0\x9F\x93\x8A TelegramP4 Status\n\n"
        "Firmware: %s\n\n"
        "WiFi:\n%s\nIP: %s\nRSSI: %d dBm\n\n"
        "Telegram:\n\xE2\x9C\x93 Connected (assumed if WiFi is up)\n\n"
        "Camera:\n%s\n\n"
        "SD Card:\n%s\nFree: %s\n\n"
        "AI:\n%s\n\n"
        "Memory:\nHeap: %" PRIu32 " bytes\nPSRAM: %s\n\n"
        "Uptime: %" PRId64 "h %" PRId64 "m %" PRId64 "s",
        TELEGRAMP4_FIRMWARE_VERSION,
        wifi_state == TELEGRAMP4_WIFI_STATE_CONNECTED ? "\xE2\x9C\x93 Connected" : "\xE2\x9D\x8C Disconnected",
        ip, rssi,
        cam.initialized ? "\xE2\x9C\x93 Ready" : "\xE2\x9D\x8C Not available",
        storage.mounted ? "\xE2\x9C\x93 Ready" : "\xE2\x9D\x8C Not mounted",
        free_str,
        telegramp4_ai_is_enabled() ? "\xE2\x9C\x93 Enabled" : "Disabled",
        (uint32_t) esp_get_free_heap_size(),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "available" : "not detected",
        uptime_s / 3600, (uptime_s / 60) % 60, uptime_s % 60);
    telegramp4_telegram_send_message(chat_id, msg);
}

/**
 * /diagnostics (spec §21). Numbers that genuinely aren't measured anywhere in
 * this firmware (camera FPS, JPEG size, AI inference time until an AI run has
 * actually happened, Telegram round-trip latency) are reported as "N/A" -
 * never fabricated.
 */
static void handler_diagnostics(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_storage_status_t storage = telegramp4_storage_get_status();
    char free_str[16] = "N/A";
    if (storage.mounted) {
        format_bytes(storage.free_bytes, free_str, sizeof(free_str));
    }
    int8_t rssi = telegramp4_wifi_get_state() == TELEGRAMP4_WIFI_STATE_CONNECTED
                    ? telegramp4_wifi_get_rssi() : 0;

    char inference_str[64];
    if (s_last_ai_inference_ms >= 0) {
        snprintf(inference_str, sizeof(inference_str), "%" PRId64 " ms (last /ai run)", s_last_ai_inference_ms);
    } else {
        snprintf(inference_str, sizeof(inference_str), "N/A (no AI run yet)");
    }

    char msg[384];
    snprintf(msg, sizeof(msg),
        "Diagnostics\n\n"
        "Free Heap: %" PRIu32 " bytes\n"
        "Free PSRAM: %s\n\n"
        "Storage free: %s\n\n"
        "Camera:\nFPS: N/A\nJPEG: N/A\n\n"
        "AI:\nInference: %s\n\n"
        "WiFi:\nRSSI: %d dBm\n\n"
        "Telegram:\nLatency: N/A (not measured)",
        (uint32_t) esp_get_free_heap_size(),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "available" : "not detected",
        free_str,
        inference_str,
        rssi);
    telegramp4_telegram_send_message(chat_id, msg);
}

/* --- OTA (Phase 21) --- */

static void handler_version(int64_t chat_id, const char *args)
{
    (void) args;
    char msg[192];
    snprintf(msg, sizeof(msg),
        "TelegramP4\n\nFirmware:\n%s\n\nESP-IDF:\n%s\n\nBoard:\n%s",
        TELEGRAMP4_FIRMWARE_VERSION, esp_get_idf_version(), TELEGRAMP4_BOARD_NAME);
    telegramp4_telegram_send_message(chat_id, msg);
}

static void handler_ota(int64_t chat_id, const char *args)
{
    if (args[0] == '\0') {
        telegramp4_telegram_send_message(chat_id, "Usage: /ota <https-url-to-firmware.bin>");
        return;
    }
    telegramp4_telegram_send_message(chat_id, "Starting OTA update. Device will reboot on success...");
    esp_err_t err = telegramp4_ota_update_from_url(args); /* does not return on success */
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C OTA update failed. Check serial log.");
    }
}

/* /photo_info (Phase 10) */
static void handler_photo_info(int64_t chat_id, const char *args)
{
    (void) args;
    if (s_last_received_filename[0] == '\0') {
        telegramp4_telegram_send_message(chat_id, "No files received yet.");
        return;
    }
    char size_str[16];
    format_bytes(s_last_received_size, size_str, sizeof(size_str));
    char msg[96];
    snprintf(msg, sizeof(msg), "Last received:\n%s\nSize: %s", s_last_received_filename, size_str);
    telegramp4_telegram_send_message(chat_id, msg);
}

/* /photo_files (Phase 10) - lists files in /sdcard/received/ */
static void handler_photo_files(int64_t chat_id, const char *args)
{
    (void) args;
    DIR *d = opendir(TELEGRAMP4_SD_MOUNT_POINT "/received");
    if (!d) {
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Storage full.\nSD card not mounted.");
        return;
    }
    char msg[256];
    int off = snprintf(msg, sizeof(msg), "Received files:\n");
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL && off < (int) sizeof(msg) && count < 15) {
        if (entry->d_type != DT_DIR) {
            off += snprintf(msg + off, sizeof(msg) - off, "%s\n", entry->d_name);
            count++;
        }
    }
    closedir(d);
    if (count == 0) {
        snprintf(msg, sizeof(msg), "No files received yet.");
    }
    telegramp4_telegram_send_message(chat_id, msg);
}

/* --- Audio (Phase 11) --- */

struct audio_task_args_t {
    int64_t chat_id;
    uint32_t duration_s;
};

static void audio_record_task(void *arg)
{
    audio_task_args_t *a = (audio_task_args_t *) arg;

    char msg[64];
    snprintf(msg, sizeof(msg), "\xF0\x9F\x8E\x99 Recording %u seconds...", (unsigned) a->duration_s);
    telegramp4_telegram_send_message(a->chat_id, msg);

    telegramp4_audio_result_t result = {0};
    esp_err_t err = telegramp4_audio_record(a->duration_s, &result);
    if (err != ESP_OK) {
        telegramp4_telegram_send_message(a->chat_id,
            "\xE2\x9D\x8C Microphone unavailable.\nCheck hardware.");
    } else {
        uint8_t *data = NULL;
        size_t len = 0;
        if (read_file_into_buffer(result.path, &data, &len)) {
            telegramp4_telegram_send_document(a->chat_id, data, len, "audio.wav");
            free(data);
        }
    }

    free(a);
    vTaskDelete(NULL);
}

/* /record [seconds] (Phase 11) */
static void handler_record(int64_t chat_id, const char *args)
{
    long seconds = (args[0] != '\0') ? atol(args) : CONFIG_TELEGRAMP4_AUDIO_DEFAULT_DURATION_S;
    if (seconds < CONFIG_TELEGRAMP4_AUDIO_MIN_DURATION_S) {
        seconds = CONFIG_TELEGRAMP4_AUDIO_MIN_DURATION_S;
    }
    if (seconds > CONFIG_TELEGRAMP4_AUDIO_MAX_DURATION_S) {
        seconds = CONFIG_TELEGRAMP4_AUDIO_MAX_DURATION_S;
    }

    auto *task_args = (audio_task_args_t *) malloc(sizeof(audio_task_args_t));
    task_args->chat_id = chat_id;
    task_args->duration_s = (uint32_t) seconds;

    /* 16KB - same HTTPS-stack-overflow reasoning as video_record_task. */
    if (xTaskCreate(audio_record_task, "audio_record", 16384, task_args, 5, NULL) != pdPASS) {
        free(task_args);
        telegramp4_telegram_send_message(chat_id, "\xE2\x9D\x8C Failed to start recording task.");
    }
}

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

/**
 * Phase 20 - periodic status refresh for the optional display. A no-op loop
 * (telegramp4_display_update_status() itself no-ops) if the display isn't
 * enabled, but only spawned when it is, to avoid wasting a task otherwise.
 * "Telegram connected" is approximated by WiFi state, since telegramp4_telegram
 * doesn't currently expose its own connectivity flag - Phase 22's fuller
 * /status work is where that could be tightened up if needed.
 */
static void display_status_task(void *arg)
{
    while (1) {
        telegramp4_display_status_t status = {0};
        status.wifi_ok = telegramp4_wifi_get_state() == TELEGRAMP4_WIFI_STATE_CONNECTED;
        status.telegram_ok = status.wifi_ok;
        status.camera_ok = telegramp4_camera_get_status().initialized;
        status.sd_ok = telegramp4_storage_get_status().mounted;
        status.ai_ok = telegramp4_ai_is_enabled();
        telegramp4_wifi_get_ip_str(status.ip, sizeof(status.ip));

        telegramp4_display_update_status(&status);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
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
    telegramp4_telegram_register_command("/photos", handler_photos);
    telegramp4_telegram_register_command("/photo_view", handler_photo_view);
    telegramp4_telegram_register_command("/photo_dl", handler_photo_download);
    telegramp4_telegram_register_command("/photo_del", handler_photo_delete_cb);
    telegramp4_telegram_register_command("/photo_info", handler_photo_info);
    telegramp4_telegram_register_command("/photo_files", handler_photo_files);
    telegramp4_telegram_register_command("/received_ai", handler_received_ai);
    telegramp4_telegram_register_command("/received_save", handler_received_save);
    telegramp4_telegram_register_command("/received_delete", handler_received_delete);
    telegramp4_telegram_set_photo_received_handler(on_photo_received);
    telegramp4_telegram_set_voice_received_handler(on_voice_received);
    telegramp4_telegram_register_command("/record", handler_record);
    telegramp4_telegram_register_command("/video", handler_video);

    telegramp4_ai_init(); /* no-op / returns error cleanly if AI disabled or unverified - see telegramp4_ai.h */
    telegramp4_telegram_register_command("/ai", handler_ai);

    esp_err_t motion_ret = telegramp4_motion_init(on_motion_detected);
    if (motion_ret != ESP_OK) {
        ESP_LOGI(TAG, "Motion detection not active: %s", esp_err_to_name(motion_ret));
    }
    telegramp4_telegram_register_command("/arm", handler_arm);
    telegramp4_telegram_register_command("/disarm", handler_disarm);
    telegramp4_telegram_register_command("/motion", handler_motion_status);

    telegramp4_gpio_init();
    telegramp4_telegram_register_command("/gpio", handler_gpio);
    telegramp4_telegram_register_command("/gpio_toggle", handler_gpio_toggle);

    telegramp4_telegram_register_command("/version", handler_version);
    telegramp4_telegram_register_command("/ota", handler_ota);

    telegramp4_telegram_register_command("/delete_confirm", handler_delete_confirm);
    telegramp4_telegram_register_command("/cancel", handler_cancel);
    telegramp4_telegram_register_command("/reboot", handler_reboot);
    telegramp4_telegram_register_command("/reboot_confirm", handler_reboot_confirm);
    telegramp4_telegram_register_command("/status", handler_full_status);
    telegramp4_telegram_register_command("/diagnostics", handler_diagnostics);

    if (telegramp4_display_init() == ESP_OK) {
        xTaskCreate(display_status_task, "display_status", 3072, NULL, 3, NULL);
    }

    esp_err_t telegram_ret = telegramp4_telegram_start();
    if (telegram_ret != ESP_OK) {
        ESP_LOGE(TAG, "Telegram bot failed to start: %s", esp_err_to_name(telegram_ret));
    }

    ESP_LOGI(TAG, "Phase 7 bootstrap complete.");
}
