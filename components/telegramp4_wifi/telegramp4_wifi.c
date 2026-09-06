#include <string.h>
#include "telegramp4_wifi.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

static const char *TAG = "TAG_WIFI";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_netif;
static volatile telegramp4_wifi_state_t s_state = TELEGRAMP4_WIFI_STATE_DISCONNECTED;

/**
 * Reconnect delay after a disconnect. A fixed short delay is simple and matches
 * the spec's "reconnect" requirement; if reconnect storms become an issue in
 * practice this can grow into exponential backoff.
 */
#define WIFI_RECONNECT_DELAY_MS 3000

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_state = TELEGRAMP4_WIFI_STATE_CONNECTING;
        ESP_LOGI(TAG, "WiFi connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_state = TELEGRAMP4_WIFI_STATE_DISCONNECTED;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected. Reconnecting...");
        vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
        s_state = TELEGRAMP4_WIFI_STATE_CONNECTING;
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        s_state = TELEGRAMP4_WIFI_STATE_CONNECTED;
        ESP_LOGI(TAG, "WiFi connected");
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t telegramp4_wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *) wifi_config.sta.ssid, CONFIG_TELEGRAMP4_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *) wifi_config.sta.password, CONFIG_TELEGRAMP4_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Never log wifi_config.sta.ssid/password contents beyond what's needed —
    // intentionally not logged here at all to avoid leaking the SSID either.
    return ESP_OK;
}

bool telegramp4_wifi_wait_connected(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

telegramp4_wifi_state_t telegramp4_wifi_get_state(void)
{
    return s_state;
}

bool telegramp4_wifi_get_ip_str(char *out, size_t out_len)
{
    if (s_state != TELEGRAMP4_WIFI_STATE_CONNECTED || !s_netif) {
        return false;
    }
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_netif, &ip_info) != ESP_OK) {
        return false;
    }
    snprintf(out, out_len, IPSTR, IP2STR(&ip_info.ip));
    return true;
}

int8_t telegramp4_wifi_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (s_state != TELEGRAMP4_WIFI_STATE_CONNECTED || esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return 0;
    }
    return ap_info.rssi;
}
