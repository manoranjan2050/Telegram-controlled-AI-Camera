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
 * Sends `text` with an arbitrary inline keyboard, given as a raw Telegram
 * `reply_markup` JSON object string (e.g. built with cJSON). Used by the
 * gallery (Phase 8) and anything else that needs a keyboard beyond the fixed
 * main menu.
 */
esp_err_t telegramp4_telegram_send_with_keyboard(int64_t chat_id, const char *text, const char *reply_markup_json);

/**
 * Uploads a JPEG image to a chat via Telegram's sendPhoto (multipart/form-data).
 * `data`/`len` must remain valid for the duration of the call. Retries once on
 * failure (spec §6: handle timeouts/retries).
 */
esp_err_t telegramp4_telegram_send_photo(int64_t chat_id, const uint8_t *data, size_t len);

/** Uploads any file as a Telegram document (sendDocument) - used for "Download". */
esp_err_t telegramp4_telegram_send_document(int64_t chat_id, const uint8_t *data, size_t len, const char *filename);

/**
 * Downloads a Telegram-hosted file by file_id (calls getFile, then downloads
 * from the file endpoint), refusing anything over `max_bytes` (checked against
 * getFile's declared size before downloading, and enforced again during the
 * download itself). On success, caller must free() *out_data.
 */
esp_err_t telegramp4_telegram_download_file(const char *file_id, size_t max_bytes,
                                             uint8_t **out_data, size_t *out_len);

/**
 * Called for every incoming message that contains a photo (Phase 10) or a
 * voice message (Phase 12), with the chat ID, the largest available file_id,
 * and its declared size in bytes (0 if unknown).
 */
typedef void (*telegramp4_media_received_cb_t)(int64_t chat_id, const char *file_id, size_t declared_size);

/** Registers the handler for incoming photo messages. Call before telegramp4_telegram_start(). */
void telegramp4_telegram_set_photo_received_handler(telegramp4_media_received_cb_t cb);

/** Registers the handler for incoming voice messages (Phase 12). */
void telegramp4_telegram_set_voice_received_handler(telegramp4_media_received_cb_t cb);

#ifdef __cplusplus
}
#endif
