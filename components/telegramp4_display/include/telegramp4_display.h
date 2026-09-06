/**
 * telegramp4_display — optional MIPI-DSI status/preview display (Phase 20).
 *
 * ⚠️ Verification status: the exact display connector/panel/driver for this
 * board has not been verified. This component is compile-time optional
 * (TELEGRAMP4_DISPLAY_ENABLED, default n) so the rest of the firmware works
 * identically whether or not a display is attached - see docs/hardware.md.
 * telegramp4_display_init() is an honest stub returning ESP_ERR_NOT_SUPPORTED
 * until a panel driver is verified.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_ok;
    bool telegram_ok;
    bool camera_ok;
    bool sd_ok;
    bool ai_ok;
    char ip[16];
} telegramp4_display_status_t;

esp_err_t telegramp4_display_init(void);
bool telegramp4_display_is_enabled(void);

/** Redraws the status screen. No-op if display isn't enabled/available. */
void telegramp4_display_update_status(const telegramp4_display_status_t *status);

#ifdef __cplusplus
}
#endif
