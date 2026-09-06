/**
 * telegramp4_security — chat-ID authorization (Phase 3) and, from Phase 7 on,
 * filename sanitization / other file-safety helpers.
 *
 * Only chat IDs listed in Kconfig's TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS may
 * control the device. This module never leaks *why* a chat is unauthorized, and
 * callers must not either — the standard response is exactly "Access denied."
 * with no further detail (see docs/security.md).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns true if `chat_id` is present in the configured whitelist.
 * Parses CONFIG_TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS on first call and caches
 * the result (the whitelist is compile-time/Kconfig-time configuration, not
 * expected to change at runtime).
 */
bool telegramp4_security_is_authorized(int64_t chat_id);

/**
 * Writes up to `max` allowed chat IDs into `out_ids`, returns the count.
 * Used by motion alerts (Phase 18) to broadcast to every authorized user.
 */
int telegramp4_security_get_allowed_ids(int64_t *out_ids, int max);

#ifdef __cplusplus
}
#endif
