/**
 * telegramp4_wifi — Wi-Fi station connectivity.
 *
 * SSID/password come from Kconfig (TelegramP4 Configuration -> WiFi), never
 * hardcoded and never logged. Handles automatic reconnect on disconnect.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TELEGRAMP4_WIFI_STATE_DISCONNECTED = 0,
    TELEGRAMP4_WIFI_STATE_CONNECTING,
    TELEGRAMP4_WIFI_STATE_CONNECTED,
} telegramp4_wifi_state_t;

/**
 * Initializes NVS-backed Wi-Fi, netif, and the default event loop, then starts
 * connecting in STA mode using the Kconfig-configured SSID/password. Non-blocking
 * — use telegramp4_wifi_wait_connected() if you need to block until the first
 * connection succeeds or times out.
 */
esp_err_t telegramp4_wifi_init(void);

/**
 * Blocks until Wi-Fi connects or `timeout_ms` elapses.
 * Returns true if connected within the timeout, false otherwise.
 */
bool telegramp4_wifi_wait_connected(uint32_t timeout_ms);

telegramp4_wifi_state_t telegramp4_wifi_get_state(void);

/** Writes the current IP address as a string (e.g. "192.168.1.50") into `out`. */
bool telegramp4_wifi_get_ip_str(char *out, size_t out_len);

/** Current RSSI in dBm, or 0 if not connected. */
int8_t telegramp4_wifi_get_rssi(void);

#ifdef __cplusplus
}
#endif
