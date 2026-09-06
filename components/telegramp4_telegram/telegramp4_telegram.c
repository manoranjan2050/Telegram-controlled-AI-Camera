#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "telegramp4_telegram.h"
#include "telegramp4_wifi.h"
#include "telegramp4_security.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

static const char *TAG = "TAG_TELEGRAM";

#define TELEGRAM_LONG_POLL_TIMEOUT_S 25
#define TELEGRAM_HTTP_TIMEOUT_MS     (35 * 1000) /* must exceed the long-poll timeout above */
#define TELEGRAM_HTTP_RESPONSE_MAX   (16 * 1024)

/* Bot API base URL, built once at startup: https://api.telegram.org/bot<token> */
static char s_api_base[8 + 5 + 32 + 64] = {0}; /* generous fixed size, token length varies */

typedef struct {
    char   *buf;
    size_t  len;
    size_t  cap;
    size_t  max; /* hard ceiling - grows are refused past this */
} http_resp_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *resp = (http_resp_buf_t *) evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && resp != NULL) {
        if (resp->len + evt->data_len + 1 > resp->cap) {
            size_t new_cap = resp->cap == 0 ? 1024 : resp->cap * 2;
            while (new_cap < resp->len + evt->data_len + 1) {
                new_cap *= 2;
            }
            if (new_cap > resp->max) {
                ESP_LOGE(TAG, "HTTP response too large (>%u bytes), aborting", (unsigned) resp->max);
                return ESP_FAIL;
            }
            char *grown = realloc(resp->buf, new_cap);
            if (!grown) {
                ESP_LOGE(TAG, "Out of memory growing HTTP response buffer");
                return ESP_FAIL;
            }
            resp->buf = grown;
            resp->cap = new_cap;
        }
        memcpy(resp->buf + resp->len, evt->data, evt->data_len);
        resp->len += evt->data_len;
        resp->buf[resp->len] = '\0';
    }
    return ESP_OK;
}

/**
 * Performs an HTTPS GET against `url`, capped at `max_bytes` of response body,
 * and returns a heap-allocated buffer (caller must free()) plus its length in
 * `out_len` if non-NULL. NUL-terminated as a convenience for JSON callers even
 * though binary downloads don't need that. Returns NULL on failure (including
 * exceeding max_bytes). TLS certificate validation uses the ESP-IDF
 * certificate bundle — never disabled.
 */
static char *http_get_ex(const char *url, int timeout_ms, size_t max_bytes, size_t *out_len)
{
    http_resp_buf_t resp = {0};
    resp.max = max_bytes;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = timeout_ms,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return NULL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        free(resp.buf);
        return NULL;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "Telegram API returned HTTP %d", status);
        free(resp.buf);
        return NULL;
    }
    if (out_len) {
        *out_len = resp.len;
    }
    return resp.buf; /* may be NULL if body was empty, which is itself an error case */
}

static char *http_get(const char *url, int timeout_ms)
{
    return http_get_ex(url, timeout_ms, TELEGRAM_HTTP_RESPONSE_MAX, NULL);
}

/** Percent-encodes `in` for safe use in a URL query parameter. */
static void url_encode(const char *in, char *out, size_t out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 4 < out_len; i++) {
        unsigned char c = (unsigned char) in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char) c;
        } else {
            out[o++] = '%';
            out[o++] = hex[(c >> 4) & 0xF];
            out[o++] = hex[c & 0xF];
        }
    }
    out[o] = '\0';
}

esp_err_t telegramp4_telegram_send_message(int64_t chat_id, const char *text)
{
    char encoded[2048];
    url_encode(text, encoded, sizeof(encoded));

    char url[3072];
    snprintf(url, sizeof(url), "%s/sendMessage?chat_id=%" PRId64 "&text=%s",
              s_api_base, chat_id, encoded);

    char *resp = http_get(url, 10 * 1000);
    if (!resp) {
        ESP_LOGE(TAG, "sendMessage failed for chat %" PRId64, chat_id);
        return ESP_FAIL;
    }
    free(resp);
    return ESP_OK;
}

#define TELEGRAM_MULTIPART_BOUNDARY "TelegramP4Boundary7f3a9c"

/**
 * Uploads `data`/`len` to Telegram method `method` (e.g. "sendPhoto",
 * "sendDocument") as multipart field `field_name`, with the given filename hint
 * and content type. Shared by send_photo/send_document so the multipart-body
 * construction exists in exactly one place.
 */
static esp_err_t upload_multipart_once(int64_t chat_id, const char *method, const char *field_name,
                                        const char *filename_hint, const char *content_type_hint,
                                        const uint8_t *data, size_t len)
{
    char part1[128];
    int part1_len = snprintf(part1, sizeof(part1),
        "--" TELEGRAM_MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
        "%" PRId64 "\r\n",
        chat_id);

    char part2[192];
    int part2_len = snprintf(part2, sizeof(part2),
        "--" TELEGRAM_MULTIPART_BOUNDARY "\r\n"
        "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
        "Content-Type: %s\r\n\r\n",
        field_name, filename_hint, content_type_hint);

    static const char part3[] = "\r\n--" TELEGRAM_MULTIPART_BOUNDARY "--\r\n";
    int part3_len = sizeof(part3) - 1;

    size_t content_length = (size_t) part1_len + (size_t) part2_len + len + (size_t) part3_len;

    char url[128];
    snprintf(url, sizeof(url), "%s/%s", s_api_base, method);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30 * 1000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", TELEGRAM_MULTIPART_BOUNDARY);
    esp_http_client_set_header(client, "Content-Type", content_type);

    esp_err_t err = esp_http_client_open(client, (int) content_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: failed to open connection: %s", method, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    /* Streamed in three pieces so the file bytes are never copied into a second
     * buffer just to build the multipart body - avoids doubling RAM usage for
     * what can be a sizeable image (see docs/memory-and-performance.md). */
    if (esp_http_client_write(client, part1, part1_len) < 0 ||
        esp_http_client_write(client, part2, part2_len) < 0 ||
        esp_http_client_write(client, (const char *) data, (int) len) < 0 ||
        esp_http_client_write(client, part3, part3_len) < 0) {
        ESP_LOGE(TAG, "%s: write failed", method);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    /* Drain and discard the response body (we only care about the status code). */
    if (content_len > 0) {
        char discard[256];
        while (esp_http_client_read(client, discard, sizeof(discard)) > 0) {
            /* discard */
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGW(TAG, "%s returned HTTP %d", method, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t upload_multipart_with_retry(int64_t chat_id, const char *method, const char *field_name,
                                              const char *filename_hint, const char *content_type_hint,
                                              const uint8_t *data, size_t len)
{
    esp_err_t err = upload_multipart_once(chat_id, method, field_name, filename_hint, content_type_hint, data, len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s failed, retrying once...", method);
        err = upload_multipart_once(chat_id, method, field_name, filename_hint, content_type_hint, data, len);
    }
    return err;
}

esp_err_t telegramp4_telegram_send_photo(int64_t chat_id, const uint8_t *data, size_t len)
{
    return upload_multipart_with_retry(chat_id, "sendPhoto", "photo", "photo.jpg", "image/jpeg", data, len);
}

esp_err_t telegramp4_telegram_send_document(int64_t chat_id, const uint8_t *data, size_t len, const char *filename)
{
    return upload_multipart_with_retry(chat_id, "sendDocument", "document", filename,
                                        "application/octet-stream", data, len);
}

esp_err_t telegramp4_telegram_download_file(const char *file_id, size_t max_bytes,
                                             uint8_t **out_data, size_t *out_len)
{
    char encoded_id[256];
    url_encode(file_id, encoded_id, sizeof(encoded_id));
    char url[512];
    snprintf(url, sizeof(url), "%s/getFile?file_id=%s", s_api_base, encoded_id);

    char *resp = http_get(url, 15 * 1000);
    if (!resp) {
        ESP_LOGE(TAG, "getFile failed for file_id %s", file_id);
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        return ESP_FAIL;
    }
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *file_path = result ? cJSON_GetObjectItem(result, "file_path") : NULL;
    cJSON *file_size = result ? cJSON_GetObjectItem(result, "file_size") : NULL;
    if (!file_path || !cJSON_IsString(file_path)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    if (file_size && (size_t) cJSON_GetNumberValue(file_size) > max_bytes) {
        ESP_LOGW(TAG, "Rejecting download: declared size %.0f exceeds max %u bytes",
                  cJSON_GetNumberValue(file_size), (unsigned) max_bytes);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    char download_url[640];
    /* Extract the token from s_api_base ("https://api.telegram.org/bot<token>")
     * rather than storing it twice - the file endpoint uses a different path
     * shape ("/file/bot<token>/<path>" vs "/bot<token>/<method>"). */
    const char *token_start = strstr(s_api_base, "/bot");
    snprintf(download_url, sizeof(download_url), "https://api.telegram.org/file%s/%s",
              token_start ? token_start : "", file_path->valuestring);
    cJSON_Delete(root);

    size_t len = 0;
    char *data = http_get_ex(download_url, 30 * 1000, max_bytes, &len);
    if (!data) {
        ESP_LOGE(TAG, "Failed to download file (over size limit or network error)");
        return ESP_FAIL;
    }
    *out_data = (uint8_t *) data;
    *out_len = len;
    return ESP_OK;
}

static void build_status_text(char *out, size_t out_len)
{
    telegramp4_wifi_state_t wifi_state = telegramp4_wifi_get_state();
    char ip[16] = "N/A";
    if (wifi_state == TELEGRAMP4_WIFI_STATE_CONNECTED) {
        telegramp4_wifi_get_ip_str(ip, sizeof(ip));
    }

    int64_t uptime_s = esp_timer_get_time() / 1000000;
    uint32_t free_heap = esp_get_free_heap_size();
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    snprintf(out, out_len,
        "TelegramP4\n"
        "\n"
        "Status:\n"
        "WiFi: %s\n"
        "IP: %s\n"
        "Uptime: %" PRId64 " sec\n"
        "Free heap: %" PRIu32 " bytes\n"
        "PSRAM: %s",
        wifi_state == TELEGRAMP4_WIFI_STATE_CONNECTED ? "Connected" : "Disconnected",
        ip,
        uptime_s,
        free_heap,
        free_psram > 0 ? "available" : "not detected");
}

/* --- Command registry (Phase 3) --- */

#define MAX_COMMANDS 32

typedef struct {
    char name[32];
    telegramp4_command_handler_t handler;
} command_entry_t;

static command_entry_t s_commands[MAX_COMMANDS];
static int s_command_count = 0;

esp_err_t telegramp4_telegram_register_command(const char *command, telegramp4_command_handler_t handler)
{
    if (s_command_count >= MAX_COMMANDS) {
        ESP_LOGE(TAG, "Command registry full, cannot register %s", command);
        return ESP_ERR_NO_MEM;
    }
    strncpy(s_commands[s_command_count].name, command, sizeof(s_commands[0].name) - 1);
    s_commands[s_command_count].handler = handler;
    s_command_count++;
    return ESP_OK;
}

static void handler_start(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_message(chat_id,
        "Welcome to TelegramP4!\n\n"
        "ESP32-P4 Telegram Camera & IoT Platform\n\n"
        "Use /help to see commands.");
    telegramp4_telegram_send_menu(chat_id);
}

static void handler_help(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_message(chat_id,
        "TelegramP4 commands:\n\n"
        "/start - welcome message\n"
        "/help - this message\n"
        "/menu - show the button menu\n"
        "/status - device status\n"
        "/photo - take a photo\n"
        "/video [seconds] - record a video\n"
        "/photos - browse recent photos\n"
        "/record [seconds] - record audio\n"
        "/ai - run AI object detection\n"
        "/files - list files on SD card\n"
        "/storage - SD card usage\n"
        "/delete <filename> - delete a photo\n"
        "/photo_info - info about the last received file\n"
        "/photo_files - list received files\n"
        "Send a photo directly to save it to the device.");
}

static void handler_menu(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_menu(chat_id);
}

static void handler_status(int64_t chat_id, const char *args)
{
    (void) args;
    char status[512];
    build_status_text(status, sizeof(status));
    telegramp4_telegram_send_message(chat_id, status);
}

/* Generic "not built yet" stub for menu items whose real phase hasn't landed. */
static void handler_not_implemented(int64_t chat_id, const char *phase_note)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "Not implemented yet - see %s.", phase_note);
    telegramp4_telegram_send_message(chat_id, msg);
}

/* /photo and /video are registered by main/app_main.cpp (Phase 6/9 onward),
 * since they need telegramp4_camera/telegramp4_video - this component doesn't
 * depend on either. */
/* /photos itself is registered by main/app_main.cpp from Phase 8 onward, since
 * it needs telegramp4_storage - this component doesn't depend on the SD module. */
/* /storage, /video, /record, /ai are registered by main/app_main.cpp, since
 * each needs a hardware component this component doesn't depend on. */
static void handler_settings_stub(int64_t chat_id, const char *args)
{
    (void) args;
    telegramp4_telegram_send_message(chat_id, "Settings are not implemented yet.");
}

static void register_builtin_commands(void)
{
    telegramp4_telegram_register_command("/start", handler_start);
    telegramp4_telegram_register_command("/help", handler_help);
    telegramp4_telegram_register_command("/menu", handler_menu);
    telegramp4_telegram_register_command("/status", handler_status);
    telegramp4_telegram_register_command("/settings", handler_settings_stub);
}

/**
 * Splits `text` (e.g. "/gpio 4 on") into a command name (up to the first space or
 * '@' — Telegram appends "@botname" to commands in group chats) and the remaining
 * argument string. Supports "/command", "/command arg", "/command arg1 arg2".
 */
static void dispatch_command(int64_t chat_id, const char *text)
{
    if (!telegramp4_security_is_authorized(chat_id)) {
        ESP_LOGW(TAG, "Unauthorized chat %" PRId64 " attempted: %s", chat_id, text);
        telegramp4_telegram_send_message(chat_id, "Access denied.");
        return;
    }

    /* Copy just the command token (stop at space or '@botname' suffix). */
    char name[32] = {0};
    size_t i = 0;
    while (text[i] != '\0' && text[i] != ' ' && text[i] != '@' && i < sizeof(name) - 1) {
        name[i] = text[i];
        i++;
    }
    name[i] = '\0';

    /* Skip past any "@botname" suffix, then any following space, to find args. */
    while (text[i] != '\0' && text[i] != ' ') {
        i++;
    }
    while (text[i] == ' ') {
        i++;
    }
    const char *args = &text[i]; /* "" if nothing follows */

    for (int c = 0; c < s_command_count; c++) {
        if (strcmp(s_commands[c].name, name) == 0) {
            s_commands[c].handler(chat_id, args);
            return;
        }
    }

    ESP_LOGI(TAG, "Unknown command from chat %" PRId64 ": %s", chat_id, name);
    telegramp4_telegram_send_message(chat_id, "Unknown command.\n\nUse /help to see available commands.");
}

/* --- Inline keyboard menu (Phase 4) --- */

static char *build_main_menu_json(void)
{
    static const char *labels[]    = {"\xF0\x9F\x93\xB8 Take Photo", "\xF0\x9F\x8E\xA5 Record Video",
                                       "\xF0\x9F\x96\xBC Last Photo", "\xF0\x9F\x8E\x99 Record Audio",
                                       "\xF0\x9F\xA4\x96 AI Detect",  "\xF0\x9F\x93\x8A Status",
                                       "\xF0\x9F\x92\xBE SD Card",    "\xE2\x9A\x99 Settings"};
    static const char *callbacks[] = {"/photo", "/video", "/photos", "/record",
                                       "/ai", "/status", "/storage", "/settings"};
    const int count = sizeof(labels) / sizeof(labels[0]);

    cJSON *root = cJSON_CreateObject();
    cJSON *keyboard = cJSON_AddArrayToObject(root, "inline_keyboard");
    for (int i = 0; i < count; i++) {
        cJSON *row = cJSON_CreateArray();
        cJSON *btn = cJSON_CreateObject();
        cJSON_AddStringToObject(btn, "text", labels[i]);
        cJSON_AddStringToObject(btn, "callback_data", callbacks[i]);
        cJSON_AddItemToArray(row, btn);
        cJSON_AddItemToArray(keyboard, row);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json; /* caller must free() */
}

esp_err_t telegramp4_telegram_send_with_keyboard(int64_t chat_id, const char *text, const char *reply_markup_json)
{
    char encoded_text[512];
    url_encode(text, encoded_text, sizeof(encoded_text));
    char encoded_markup[2048];
    url_encode(reply_markup_json, encoded_markup, sizeof(encoded_markup));

    char url[3072];
    snprintf(url, sizeof(url), "%s/sendMessage?chat_id=%" PRId64 "&text=%s&reply_markup=%s",
              s_api_base, chat_id, encoded_text, encoded_markup);

    char *resp = http_get(url, 10 * 1000);
    if (!resp) {
        ESP_LOGE(TAG, "sendMessage-with-keyboard failed for chat %" PRId64, chat_id);
        return ESP_FAIL;
    }
    free(resp);
    return ESP_OK;
}

esp_err_t telegramp4_telegram_send_menu(int64_t chat_id)
{
    char *menu_json = build_main_menu_json();
    if (!menu_json) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = telegramp4_telegram_send_with_keyboard(chat_id, "TelegramP4", menu_json);
    free(menu_json);
    return err;
}

static void answer_callback_query(const char *callback_query_id)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/answerCallbackQuery?callback_query_id=%s",
              s_api_base, callback_query_id);
    char *resp = http_get(url, 10 * 1000);
    if (resp) {
        free(resp);
    }
}

static void handle_callback_query(cJSON *cq)
{
    cJSON *data = cJSON_GetObjectItem(cq, "data");
    cJSON *id = cJSON_GetObjectItem(cq, "id");
    cJSON *message = cJSON_GetObjectItem(cq, "message");
    if (!data || !cJSON_IsString(data) || !message) {
        return;
    }
    cJSON *chat = cJSON_GetObjectItem(message, "chat");
    cJSON *chat_id_json = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
    if (!chat_id_json) {
        return;
    }
    int64_t chat_id = (int64_t) cJSON_GetNumberValue(chat_id_json);

    ESP_LOGI(TAG, "Received callback from chat %" PRId64 ": %s", chat_id, data->valuestring);
    /* Same dispatch path as a typed command - buttons never duplicate handler logic. */
    dispatch_command(chat_id, data->valuestring);

    if (id && cJSON_IsString(id)) {
        answer_callback_query(id->valuestring);
    }
}

/* --- Incoming media (Phase 10/12) --- */

static telegramp4_media_received_cb_t s_photo_cb = NULL;
static telegramp4_media_received_cb_t s_voice_cb = NULL;

void telegramp4_telegram_set_photo_received_handler(telegramp4_media_received_cb_t cb) { s_photo_cb = cb; }
void telegramp4_telegram_set_voice_received_handler(telegramp4_media_received_cb_t cb) { s_voice_cb = cb; }

/** Telegram sends "photo" as an array of PhotoSize, smallest to largest - take the last. */
static void handle_incoming_photo(int64_t chat_id, cJSON *photo_array)
{
    if (!s_photo_cb || !cJSON_IsArray(photo_array)) {
        return;
    }
    int n = cJSON_GetArraySize(photo_array);
    if (n == 0) {
        return;
    }
    cJSON *largest = cJSON_GetArrayItem(photo_array, n - 1);
    cJSON *file_id = cJSON_GetObjectItem(largest, "file_id");
    cJSON *file_size = cJSON_GetObjectItem(largest, "file_size");
    if (!file_id || !cJSON_IsString(file_id)) {
        return;
    }
    size_t size = file_size ? (size_t) cJSON_GetNumberValue(file_size) : 0;
    s_photo_cb(chat_id, file_id->valuestring, size, 0);
}

static void handle_incoming_voice(int64_t chat_id, cJSON *voice_obj)
{
    if (!s_voice_cb) {
        return;
    }
    cJSON *file_id = cJSON_GetObjectItem(voice_obj, "file_id");
    cJSON *file_size = cJSON_GetObjectItem(voice_obj, "file_size");
    cJSON *duration = cJSON_GetObjectItem(voice_obj, "duration");
    if (!file_id || !cJSON_IsString(file_id)) {
        return;
    }
    size_t size = file_size ? (size_t) cJSON_GetNumberValue(file_size) : 0;
    uint32_t duration_s = duration ? (uint32_t) cJSON_GetNumberValue(duration) : 0;
    s_voice_cb(chat_id, file_id->valuestring, size, duration_s);
}

void telegramp4_telegram_dispatch(int64_t chat_id, const char *command_text)
{
    dispatch_command(chat_id, command_text);
}

static void process_update(cJSON *update)
{
    cJSON *callback_query = cJSON_GetObjectItem(update, "callback_query");
    if (callback_query) {
        handle_callback_query(callback_query);
        return;
    }

    cJSON *message = cJSON_GetObjectItem(update, "message");
    if (!message) {
        return; /* not a text message update (could be a callback query, edited message, etc.) */
    }
    cJSON *chat = cJSON_GetObjectItem(message, "chat");
    cJSON *chat_id_json = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
    if (!chat_id_json) {
        return;
    }
    int64_t chat_id = (int64_t) cJSON_GetNumberValue(chat_id_json);

    if (!telegramp4_security_is_authorized(chat_id)) {
        /* Silently ignore non-command media/messages from unauthorized chats -
         * dispatch_command() handles the "Access denied." reply for text
         * commands specifically; unsolicited photo/voice uploads from
         * strangers shouldn't get any response at all. */
        cJSON *text_probe = cJSON_GetObjectItem(message, "text");
        if (text_probe && cJSON_IsString(text_probe) && text_probe->valuestring[0] == '/') {
            dispatch_command(chat_id, text_probe->valuestring);
        }
        return;
    }

    cJSON *text = cJSON_GetObjectItem(message, "text");
    if (text && cJSON_IsString(text) && text->valuestring[0] == '/') {
        dispatch_command(chat_id, text->valuestring);
        return;
    }

    cJSON *photo = cJSON_GetObjectItem(message, "photo");
    if (photo) {
        handle_incoming_photo(chat_id, photo);
        return;
    }

    cJSON *voice = cJSON_GetObjectItem(message, "voice");
    if (voice) {
        handle_incoming_voice(chat_id, voice);
        return;
    }
}

static void telegram_poll_task(void *arg)
{
    int64_t offset = 0;

    while (1) {
        if (telegramp4_wifi_get_state() != TELEGRAMP4_WIFI_STATE_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        char url[256];
        snprintf(url, sizeof(url), "%s/getUpdates?offset=%" PRId64 "&timeout=%d",
                  s_api_base, offset, TELEGRAM_LONG_POLL_TIMEOUT_S);

        char *resp = http_get(url, TELEGRAM_HTTP_TIMEOUT_MS);
        if (!resp) {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        cJSON *root = cJSON_Parse(resp);
        free(resp);
        if (!root) {
            ESP_LOGW(TAG, "Failed to parse getUpdates response");
            continue;
        }

        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (cJSON_IsArray(result)) {
            cJSON *update;
            cJSON_ArrayForEach(update, result) {
                cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
                if (update_id) {
                    int64_t id = (int64_t) cJSON_GetNumberValue(update_id);
                    if (id + 1 > offset) {
                        offset = id + 1;
                    }
                }
                process_update(update);
            }
        }
        cJSON_Delete(root);
    }
}

esp_err_t telegramp4_telegram_start(void)
{
    register_builtin_commands();

    snprintf(s_api_base, sizeof(s_api_base), "https://api.telegram.org/bot%s",
              CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN);

    if (strlen(CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN) == 0 ||
        strcmp(CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN, "123456789:REPLACE_WITH_YOUR_BOT_TOKEN") == 0) {
        ESP_LOGE(TAG, "Telegram bot token not configured. Run `idf.py menuconfig` -> "
                       "TelegramP4 Configuration -> Telegram.");
        return ESP_ERR_INVALID_STATE;
    }

    /* getMe confirms the token is valid without ever logging the token itself. */
    char url[128];
    snprintf(url, sizeof(url), "%s/getMe", s_api_base);
    char *resp = http_get(url, 10 * 1000);
    if (!resp) {
        ESP_LOGE(TAG, "getMe failed - check WiFi connectivity and bot token validity");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (root) {
        cJSON *ok = cJSON_GetObjectItem(root, "ok");
        cJSON *result = cJSON_GetObjectItem(root, "result");
        cJSON *username = result ? cJSON_GetObjectItem(result, "username") : NULL;
        if (cJSON_IsTrue(ok) && username && cJSON_IsString(username)) {
            ESP_LOGI(TAG, "Telegram bot connected: @%s", username->valuestring);
        } else {
            ESP_LOGE(TAG, "getMe did not return ok=true - check bot token");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        cJSON_Delete(root);
    }

    BaseType_t ok = xTaskCreate(telegram_poll_task, "telegram_poll", 8192, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telegram_poll_task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
