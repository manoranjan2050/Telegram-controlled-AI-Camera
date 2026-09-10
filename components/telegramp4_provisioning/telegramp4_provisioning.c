#include <string.h>
#include <stdlib.h>
#include "telegramp4_provisioning.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TAG_PROVISION";

#define NVS_NAMESPACE   "tp4cfg"
#define SETUP_AP_SSID   "TelegramP4-Setup"

/* Minimal in-place URL/form decoder: '+' -> space, '%XX' -> byte. Safe to
 * decode into the same buffer since the decoded string is never longer
 * than the encoded one. */
static void url_decode(char *s)
{
    char *out = s;
    while (*s) {
        if (*s == '+') {
            *out++ = ' ';
            s++;
        } else if (*s == '%' && s[1] && s[2]) {
            char hex[3] = { s[1], s[2], '\0' };
            *out++ = (char) strtol(hex, NULL, 16);
            s += 3;
        } else {
            *out++ = *s++;
        }
    }
    *out = '\0';
}

bool telegramp4_provisioning_load(telegramp4_provisioning_config_t *out)
{
    memset(out, 0, sizeof(*out));

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        size_t len;
        len = sizeof(out->wifi_ssid);
        nvs_get_str(h, "ssid", out->wifi_ssid, &len);
        len = sizeof(out->wifi_password);
        nvs_get_str(h, "pass", out->wifi_password, &len);
        len = sizeof(out->bot_token);
        nvs_get_str(h, "token", out->bot_token, &len);
        len = sizeof(out->chat_ids);
        nvs_get_str(h, "chats", out->chat_ids, &len);
        nvs_close(h);

        if (strlen(out->wifi_ssid) > 0 && strlen(out->bot_token) > 0) {
            ESP_LOGI(TAG, "Loaded configuration from NVS (set via the setup portal)");
            return true;
        }
    }

    /* No saved config yet - fall back to Kconfig, for developers who still
     * bake real credentials into sdkconfig for bench/CI testing. This keeps
     * that existing workflow working unchanged; only an out-of-box device
     * with neither NVS nor Kconfig configured falls through to the portal. */
    if (strlen(CONFIG_TELEGRAMP4_WIFI_SSID) > 0 &&
        strlen(CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN) > 0 &&
        strcmp(CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN, "123456789:REPLACE_WITH_YOUR_BOT_TOKEN") != 0) {
        strncpy(out->wifi_ssid, CONFIG_TELEGRAMP4_WIFI_SSID, sizeof(out->wifi_ssid) - 1);
        strncpy(out->wifi_password, CONFIG_TELEGRAMP4_WIFI_PASSWORD, sizeof(out->wifi_password) - 1);
        strncpy(out->bot_token, CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN, sizeof(out->bot_token) - 1);
        strncpy(out->chat_ids, CONFIG_TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS, sizeof(out->chat_ids) - 1);
        ESP_LOGI(TAG, "Loaded configuration from Kconfig (sdkconfig) defaults");
        return true;
    }

    return false;
}

void telegramp4_provisioning_clear(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}

static esp_err_t save_config(const telegramp4_provisioning_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    nvs_set_str(h, "ssid", cfg->wifi_ssid);
    nvs_set_str(h, "pass", cfg->wifi_password);
    nvs_set_str(h, "token", cfg->bot_token);
    nvs_set_str(h, "chats", cfg->chat_ids);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static const char kSetupPage[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>TelegramP4 Setup</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px;color:#222}"
    "h1{font-size:20px}"
    "label{display:block;margin-top:14px;font-weight:bold;font-size:14px}"
    "input{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;font-size:14px}"
    "button{margin-top:20px;width:100%;padding:10px;font-size:15px;background:#0088cc;color:#fff;border:none;border-radius:4px}"
    ".hint{color:#666;font-size:12px;margin-top:2px}"
    "</style></head><body>"
    "<h1>\xF0\x9F\x93\xB7 TelegramP4 Setup</h1>"
    "<p>Connect your ESP32-P4 camera to WiFi and your Telegram bot.</p>"
    "<form method=\"POST\" action=\"/save\">"
    "<label>WiFi Network Name (SSID)</label>"
    "<input name=\"ssid\" required maxlength=\"32\">"
    "<label>WiFi Password</label>"
    "<input name=\"pass\" type=\"password\" maxlength=\"64\">"
    "<label>Telegram Bot Token</label>"
    "<input name=\"token\" required maxlength=\"63\">"
    "<div class=\"hint\">Get this from @BotFather on Telegram.</div>"
    "<label>Your Telegram Chat ID</label>"
    "<input name=\"chats\" required maxlength=\"127\">"
    "<div class=\"hint\">Message @userinfobot on Telegram to find your chat ID. "
    "Multiple IDs: separate with commas.</div>"
    "<button type=\"submit\">Save &amp; Restart</button>"
    "</form></body></html>";

static const char kSavedPage[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Saved</title>"
    "<style>body{font-family:sans-serif;max-width:420px;margin:60px auto;padding:0 16px;text-align:center;color:#222}</style>"
    "</head><body><h1>\xE2\x9C\x85 Saved!</h1>"
    "<p>The device is restarting and will connect to your WiFi network. "
    "This setup page will stop working once it disconnects from "
    "\"" SETUP_AP_SSID "\".</p></body></html>";

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kSetupPage, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_post_save(httpd_req_t *req)
{
    char body[512] = {0};
    int total = 0;
    while (total < req->content_len && total < (int) sizeof(body) - 1) {
        int r = httpd_req_recv(req, body + total, sizeof(body) - 1 - total);
        if (r <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        total += r;
    }
    body[total] = '\0';

    telegramp4_provisioning_config_t cfg = {0};
    httpd_query_key_value(body, "ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
    httpd_query_key_value(body, "pass", cfg.wifi_password, sizeof(cfg.wifi_password));
    httpd_query_key_value(body, "token", cfg.bot_token, sizeof(cfg.bot_token));
    httpd_query_key_value(body, "chats", cfg.chat_ids, sizeof(cfg.chat_ids));
    url_decode(cfg.wifi_ssid);
    url_decode(cfg.wifi_password);
    url_decode(cfg.bot_token);
    url_decode(cfg.chat_ids);

    if (strlen(cfg.wifi_ssid) == 0 || strlen(cfg.bot_token) == 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Missing required field.", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    esp_err_t err = save_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save configuration: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Configuration saved via setup portal. Restarting...");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kSavedPage, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

void telegramp4_provisioning_run_portal(void)
{
    ESP_LOGW(TAG, "Starting setup portal - connect to WiFi network \"%s\" and open http://192.168.4.1/",
              SETUP_AP_SSID);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = SETUP_AP_SSID,
            .ssid_len = strlen(SETUP_AP_SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.lru_purge_enable = true;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &http_config));

    httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = handle_get_root };
    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = handle_post_save };
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &save_uri);

    /* This task has nothing left to do - the HTTP server runs on its own
     * task and esp_restart() (called from handle_post_save) is what ends
     * setup mode. Idle here forever rather than returning, so the caller
     * never proceeds into normal startup with no configuration. */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
