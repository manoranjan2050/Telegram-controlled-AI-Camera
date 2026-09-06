/**
 * telegramp4_telegram — Telegram Bot API client.
 *
 * Phase 2 scope: HTTPS client (TLS cert validation via the ESP-IDF certificate
 * bundle — never disabled), getMe/getUpdates long polling, and the three hardcoded
 * commands /start /help /status. The reusable command registry and chat-ID
 * authorization arrive in Phase 3 — every chat is treated as authorized for now.
 *
 * The bot token is read from Kconfig (CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN) and is
 * never logged or returned by any function in this header.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Verifies the bot token works (calls getMe) and starts the long-polling task.
 * Requires WiFi to already be connected (or connecting) — the poll loop will
 * simply keep getting HTTP errors and retry if WiFi drops, it does not manage
 * WiFi itself.
 */
esp_err_t telegramp4_telegram_start(void);

/** Sends a plain-text message to the given chat ID. Returns ESP_OK on HTTP 200. */
esp_err_t telegramp4_telegram_send_message(int64_t chat_id, const char *text);

#ifdef __cplusplus
}
#endif
