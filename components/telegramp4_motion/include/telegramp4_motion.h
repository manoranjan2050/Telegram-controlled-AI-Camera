/**
 * telegramp4_motion — external PIR sensor on a whitelisted GPIO (Phase 17).
 *
 * ⚠️ Verification status: which GPIOs are safe/available on the FireBeetle 2
 * ESP32-P4 (not already claimed by camera/SD/display) has not been verified.
 * TELEGRAMP4_MOTION_PIR_GPIO defaults to -1 (disabled/unset) precisely so
 * nobody accidentally enables an interrupt on a pin that's actually wired to
 * something else - you must explicitly set a real, confirmed-safe GPIO number
 * in Kconfig before enabling this. Also assumes the common PIR behavior of
 * driving the output HIGH on motion (e.g. HC-SR501-style modules) - confirm
 * this against whatever PIR module you actually use; some are active-low.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*telegramp4_motion_cb_t)(void);

/**
 * Configures the PIR GPIO and starts a background task that calls `cb`
 * (never from ISR context) whenever motion is detected *and* the system is
 * armed. No-op / returns ESP_ERR_NOT_SUPPORTED if motion detection is
 * disabled or no GPIO is configured in Kconfig.
 */
esp_err_t telegramp4_motion_init(telegramp4_motion_cb_t cb);

void telegramp4_motion_arm(void);
void telegramp4_motion_disarm(void);
bool telegramp4_motion_is_armed(void);

#ifdef __cplusplus
}
#endif
