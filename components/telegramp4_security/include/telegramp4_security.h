/**
 * telegramp4_security — chat-ID authorization (Phase 3) and, from Phase 7 on,
 * filename sanitization / other file-safety helpers.
 *
 * Only whitelisted chat IDs may control the device (set via the setup portal
 * / telegramp4_provisioning, or Kconfig's TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS
 * as a developer fallback — see telegramp4_security_configure()). This module
 * never leaks *why* a chat is unauthorized, and callers must not either — the
 * standard response is exactly "Access denied." with no further detail (see
 * docs/security.md).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sets the comma-separated chat-ID whitelist to use, overriding the Kconfig
 * default. Call once at startup (main/app_main.cpp), before any command can
 * be dispatched. Passing NULL or "" falls back to Kconfig's
 * TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS.
 */
void telegramp4_security_configure(const char *chat_ids_csv);

/**
 * Returns true if `chat_id` is present in the configured whitelist.
 * Parses the whitelist (set via telegramp4_security_configure(), or Kconfig
 * if never called) on first check and caches the result.
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
