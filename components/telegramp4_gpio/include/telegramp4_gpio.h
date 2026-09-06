/**
 * telegramp4_gpio — whitelisted GPIO control (Phase 19).
 *
 * Only pins listed in TELEGRAMP4_GPIO_WHITELIST (Kconfig, comma-separated)
 * can be controlled. Never exposes arbitrary/dangerous pins automatically —
 * see docs/hardware.md for which pins are actually free to use on this board
 * (unconfirmed as of this writing).
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEGRAMP4_GPIO_MAX_WHITELISTED 16

/** Parses the whitelist from Kconfig and configures each pin as an output. */
esp_err_t telegramp4_gpio_init(void);

/** Number of whitelisted pins, and their numbers (up to TELEGRAMP4_GPIO_MAX_WHITELISTED). */
int telegramp4_gpio_get_whitelist(int *out_pins, int max);

/** True if `pin` is in the configured whitelist. */
bool telegramp4_gpio_is_whitelisted(int pin);

/** Sets a whitelisted pin high/low. Returns ESP_ERR_NOT_FOUND if not whitelisted. */
esp_err_t telegramp4_gpio_set(int pin, bool on);

/** Returns the last-set level of a whitelisted pin. */
bool telegramp4_gpio_get(int pin);

#ifdef __cplusplus
}
#endif
