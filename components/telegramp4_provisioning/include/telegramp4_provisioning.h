/**
 * telegramp4_provisioning — first-time setup web dashboard.
 *
 * Lets an end user flash the firmware once with no WiFi/Telegram credentials
 * baked in at build time, then configure the device entirely over a web
 * page: it starts a SoftAP + HTTP server, the user connects a phone/laptop
 * to it and fills in a form, and the values are saved to NVS and used on
 * every subsequent boot — no `idf.py menuconfig` or rebuild required.
 *
 * Developers who prefer the existing Kconfig-based workflow (real values
 * baked into sdkconfig, e.g. for CI or bench testing) are unaffected:
 * telegramp4_provisioning_load() falls back to those Kconfig values when
 * NVS has nothing saved yet, so the portal never interrupts that workflow.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char bot_token[64];
    char chat_ids[128]; /* comma-separated Telegram chat IDs */
} telegramp4_provisioning_config_t;

/**
 * Loads the device's configuration: first tries NVS (namespace "tp4cfg",
 * written by the setup portal), then falls back to Kconfig defaults if NVS
 * is empty but the build has real (non-placeholder) values baked in.
 * Returns true if a usable configuration (SSID + bot token both non-empty)
 * was found, false if the device needs to be provisioned.
 */
bool telegramp4_provisioning_load(telegramp4_provisioning_config_t *out);

/**
 * Erases any saved NVS configuration - used to force re-provisioning (e.g.
 * holding the BOOT button at power-on).
 */
void telegramp4_provisioning_clear(void);

/**
 * Starts a SoftAP ("TelegramP4-Setup", open) and a web server serving a
 * setup form at http://192.168.4.1/. On submit, saves the values to NVS
 * and reboots the device via esp_restart(). Never returns normally - call
 * this instead of proceeding with normal startup when
 * telegramp4_provisioning_load() returns false.
 */
void telegramp4_provisioning_run_portal(void);

#ifdef __cplusplus
}
#endif
