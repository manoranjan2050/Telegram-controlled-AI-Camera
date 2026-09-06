# Lesson 03 — Telegram `/start`

## Goal

Talk to your ESP32-P4 from Telegram for the first time: `/start`, `/help`,
`/status`.

## What you learn

- Calling the Telegram Bot API over HTTPS from ESP-IDF with `esp_http_client`
- Validating TLS certificates using the ESP-IDF certificate bundle (never
  disabling verification)
- Long polling via `getUpdates` and why it's simpler than webhooks for a
  microcontroller with no public IP
- Parsing JSON responses with cJSON
- Keeping the bot token out of logs and out of Git

## Hardware

Same as Lesson 02, plus a Telegram bot (see
[telegram-bot-setup.md](../telegram-bot-setup.md)).

## Wiring

None.

## Software

Same as previous lessons. No new host-side tools.

## Code

- [`components/telegramp4_telegram/`](../../components/telegramp4_telegram/) — new
  component: `telegramp4_telegram_start()`, `telegramp4_telegram_send_message()`
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `Telegram` submenu:
  `TELEGRAMP4_TELEGRAM_BOT_TOKEN`
- [`sdkconfig.defaults`](../../sdkconfig.defaults) — enables the mbedTLS
  certificate bundle used for TLS validation
- [`main/app_main.cpp`](../../main/app_main.cpp) — starts the Telegram client
  after WiFi

## How it works

On startup, `telegramp4_telegram_start()` builds the Bot API base URL from your
configured token and calls `getMe` once to confirm the token is valid — it logs
the bot's `@username`, never the token. It then spawns a background task that
loops forever calling `getUpdates` with a 25-second long-poll timeout: Telegram
holds the HTTP connection open and responds as soon as a new message arrives (or
after the timeout with an empty result), which avoids constant short-interval
polling.

Each update is parsed with cJSON. If it contains a text message, the chat ID and
text are dispatched to a simple `if`/`else` command handler (`/start`, `/help`,
`/status`) — this is intentionally not a registry yet; that generalization is
Phase 3. `/status` reports WiFi state, IP, uptime (`esp_timer_get_time()`), free
heap, and whether PSRAM was detected.

**No chat-ID authorization yet** — any chat that messages the bot gets a response.
This is fine for private development testing but is not the final security model;
Phase 3 adds the whitelist. Don't make the bot token or bot username public while
testing without a chat-ID whitelist in place.

## Test

```bash
idf.py menuconfig   # TelegramP4 Configuration -> Telegram -> paste your bot token
idf.py build
idf.py -p <PORT> flash monitor
```

In Telegram, message your bot: `/start`, then `/status`, then `/help`.

## Expected output

Serial monitor:
```
I (xxx) TAG_TELEGRAM: Telegram bot connected: @YourBotName
```
Telegram chat:
```
/start
Welcome to TelegramP4!

ESP32-P4 Telegram Camera & IoT Platform

Use /help to see commands.
```
```
/status
TelegramP4

Status:
WiFi: Connected
IP: 192.168.1.50
Uptime: 42 sec
Free heap: 8123456 bytes
PSRAM: not detected
```
("PSRAM: not detected" is expected until PSRAM is enabled/verified in Phase 5.)

## Common errors

- **`getMe failed`** — check the token, and confirm WiFi actually connected first
  (check Lesson 02's expected log output).
- **No response at all in Telegram** — check the serial log for HTTP error codes;
  a wrong system time (no NTP) can break TLS certificate validation.
- See [docs/troubleshooting.md](../troubleshooting.md).

## Challenge

Add an `/id` command (not in the final command list, just for learning) that
replies with the sender's chat ID — handy for filling in the Phase 3 whitelist
without needing the `getUpdates` browser trick from the setup guide.

## Phase 3 update — command registry and chat-ID authorization

The `if`/`else` dispatch above is now a real registry:
`telegramp4_telegram_register_command("/name", handler)`. Built-in commands
(`/start`, `/help`, `/status`, plus a `/photo` stub replying "Not implemented yet
- see Phase 6") are registered internally before the poll task starts. Handlers
have the signature `void handler(int64_t chat_id, const char *args)`, so
`/gpio 4 on` arrives as command `/gpio` with `args = "4 on"`.

**Every command now goes through `telegramp4_security_is_authorized()` first.**
Chat IDs not in `TELEGRAMP4_TELEGRAM_ALLOWED_CHAT_IDS` (Kconfig) get exactly:
```
Access denied.
```
and nothing else — the new `telegramp4_security` component never reveals *why* or
*what* the device could otherwise do. Unrecognized commands from authorized chats
get:
```
Unknown command.

Use /help to see available commands.
```

Set your chat ID before testing:
```bash
idf.py menuconfig   # TelegramP4 Configuration -> Telegram -> Allowed Telegram Chat IDs
```
If you leave it empty, every command is denied — that's intentional, not a bug.

## Next lesson

[Lesson 04 — Telegram inline buttons](04-telegram-buttons.md)
