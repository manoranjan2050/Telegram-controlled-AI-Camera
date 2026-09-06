/**
 * telegramp4_telegram — Telegram Bot API client and command dispatch.
 *
 * HTTPS client (TLS cert validation via the ESP-IDF certificate bundle — never
 * disabled), getMe/getUpdates long polling, a reusable command registry
 * (Phase 3), and chat-ID authorization enforced before any handler runs
 * (Phase 3, via telegramp4_security).
 *
 * The bot token is read from Kconfig (CONFIG_TELEGRAMP4_TELEGRAM_BOT_TOKEN) and is
 * never logged or returned by any function in this header.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A command handler. `args` points at the text after the command name (with
 * leading whitespace stripped), or "" if no arguments were given. `args` is only
 * valid for the duration of the call.
 */
typedef void (*telegramp4_command_handler_t)(int64_t chat_id, const char *args);

/**
 * Registers a handler for `command` (must include the leading "/", e.g.
 * "/photo"). Must be called before telegramp4_telegram_start(). Buttons
 * (Phase 4) invoke the exact same registered handlers — never duplicate this
 * logic in a callback-query handler.
 */
esp_err_t telegramp4_telegram_register_command(const char *command, telegramp4_command_handler_t handler);

/**
 * Verifies the bot token works (calls getMe) and starts the long-polling task.
 * Requires WiFi to already be connected (or connecting) — the poll loop will
 * simply keep getting HTTP errors and retry if WiFi drops, it does not manage
 * WiFi itself. Register all commands before calling this.
 */
esp_err_t telegramp4_telegram_start(void);

/** Sends a plain-text message to the given chat ID. Returns ESP_OK on HTTP 200. */
esp_err_t telegramp4_telegram_send_message(int64_t chat_id, const char *text);

/**
 * Sends the main inline-keyboard menu (Phase 4). Each button's callback_data is
 * a command name (e.g. "/photo") so callback queries are dispatched through the
 * exact same registered handlers as typed commands — see
 * telegramp4_telegram_register_command().
 */
esp_err_t telegramp4_telegram_send_menu(int64_t chat_id);

/**
 * Uploads a JPEG image to a chat via Telegram's sendPhoto (multipart/form-data).
 * `data`/`len` must remain valid for the duration of the call. Retries once on
 * failure (spec §6: handle timeouts/retries).
 */
esp_err_t telegramp4_telegram_send_photo(int64_t chat_id, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
