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
            if (new_cap > TELEGRAM_HTTP_RESPONSE_MAX) {
                ESP_LOGE(TAG, "HTTP response too large (>%d bytes), truncating", TELEGRAM_HTTP_RESPONSE_MAX);
                return ESP_OK;
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
 * Performs an HTTPS GET against `url` and returns a heap-allocated, NUL-terminated
 * response body (caller must free()), or NULL on failure. TLS certificate
 * validation uses the ESP-IDF certificate bundle — never disabled.
 */
static char *http_get(const char *url, int timeout_ms)
{
    http_resp_buf_t resp = {0};

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
    return resp.buf; /* may be NULL if body was empty, which is itself an error case */
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
        "/photo - not implemented yet (Phase 6)\n"
        "/video - not implemented yet (Phase 9)\n"
        "/photos - not implemented yet (Phase 8)\n"
        "/record - not implemented yet (Phase 11)\n"
        "/ai - not implemented yet (Phase 15)\n"
        "/storage - not implemented yet (Phase 7)");
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

static void handler_photo_stub(int64_t chat_id, const char *args)   { (void) args; handler_not_implemented(chat_id, "Phase 6"); }
static void handler_video_stub(int64_t chat_id, const char *args)   { (void) args; handler_not_implemented(chat_id, "Phase 9"); }
static void handler_photos_stub(int64_t chat_id, const char *args)  { (void) args; handler_not_implemented(chat_id, "Phase 8"); }
static void handler_record_stub(int64_t chat_id, const char *args)  { (void) args; handler_not_implemented(chat_id, "Phase 11"); }
static void handler_ai_stub(int64_t chat_id, const char *args)      { (void) args; handler_not_implemented(chat_id, "Phase 15"); }
static void handler_storage_stub(int64_t chat_id, const char *args) { (void) args; handler_not_implemented(chat_id, "Phase 7"); }
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
    telegramp4_telegram_register_command("/photo", handler_photo_stub);
    telegramp4_telegram_register_command("/video", handler_video_stub);
    telegramp4_telegram_register_command("/photos", handler_photos_stub);
    telegramp4_telegram_register_command("/record", handler_record_stub);
    telegramp4_telegram_register_command("/ai", handler_ai_stub);
    telegramp4_telegram_register_command("/storage", handler_storage_stub);
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

esp_err_t telegramp4_telegram_send_menu(int64_t chat_id)
{
    char *menu_json = build_main_menu_json();
    if (!menu_json) {
        return ESP_ERR_NO_MEM;
    }

    char encoded_markup[2048];
    url_encode(menu_json, encoded_markup, sizeof(encoded_markup));
    free(menu_json);

    char url[3072];
    snprintf(url, sizeof(url), "%s/sendMessage?chat_id=%" PRId64 "&text=TelegramP4&reply_markup=%s",
              s_api_base, chat_id, encoded_markup);

    char *resp = http_get(url, 10 * 1000);
    if (!resp) {
        ESP_LOGE(TAG, "sendMenu failed for chat %" PRId64, chat_id);
        return ESP_FAIL;
    }
    free(resp);
    return ESP_OK;
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
    cJSON *text = cJSON_GetObjectItem(message, "text");
    if (!chat || !text || !cJSON_IsString(text)) {
        return;
    }
    cJSON *chat_id_json = cJSON_GetObjectItem(chat, "id");
    if (!chat_id_json) {
        return;
    }
    int64_t chat_id = (int64_t) cJSON_GetNumberValue(chat_id_json);

    ESP_LOGI(TAG, "Received message from chat %" PRId64 ": %s", chat_id, text->valuestring);
    if (text->valuestring[0] == '/') {
        dispatch_command(chat_id, text->valuestring);
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
