# Troubleshooting

## Build

**`idf.py build` fails with "unsupported target"**
Run `idf.py set-target esp32p4` first, and confirm your ESP-IDF version supports
`esp32p4` via `idf.py --list-targets`.

**Component not found / missing dependency**
Run `idf.py fullclean` then `idf.py build` — a stale build directory or
`managed_components/` cache can cause this after adding a new component.

## Flashing

**Flash fails / port not found**
Check the correct serial port (`idf.py -p <PORT> flash`), that no other program
(monitor, Arduino IDE) is holding the port open, and that the board is in the
correct boot mode if it doesn't auto-reset.

## Wi-Fi

**`WiFi connecting...` never resolves**
Double-check SSID/password in `idf.py menuconfig` (they're case-sensitive), confirm
the network is 2.4 GHz if the board doesn't support 5 GHz, and check
`WIFI_CONNECT_TIMEOUT_MS`.

## Telegram

**Bot doesn't respond to `/start`**
- Confirm the bot token is correct (`idf.py menuconfig` → Telegram).
- Confirm your chat ID is in `TELEGRAM_ALLOWED_CHAT_IDS` — an unlisted chat gets no
  meaningful response by design (`Access denied.` at most, per module).
- Confirm Wi-Fi is actually connected (check `/status` locally via serial monitor
  logs, since `/status` itself requires Telegram to already be working).
- Check the serial monitor for HTTPS/TLS errors — a wrong system time (no NTP sync)
  can cause certificate validation failures.

**"Access denied." for a chat ID you believe is correct**
Re-fetch your chat ID via the `getUpdates` method described in
[telegram-bot-setup.md](telegram-bot-setup.md) — group chats and channels have
different ID formats than private chats.

## Camera

**Camera init fails**
Verify the camera module/ribbon is correctly seated, check
[hardware.md](hardware.md) for the confirmed connector/pin mapping for this
firmware version, and check serial logs for the specific error code.

## MicroSD

**SD mount fails**
Confirm the card is formatted FAT32, check the confirmed SD interface/pins in
[hardware.md](hardware.md), and try a different/known-good card.

## AI

**AI inference fails or is disabled**
Check `AI_ENABLED` in Kconfig, and check free PSRAM via `/status` or `/diagnostics`
— insufficient memory is a common cause reported as
`❌ AI inference failed: insufficient memory`.

## General

If a subsystem reports an error message, that message is the authoritative
diagnostic — the firmware never silently swallows serious errors (spec §12). Check
the serial monitor log around the same timestamp for the underlying ESP-IDF error
code.

This document grows with each phase — add real issues encountered during hardware
testing, not speculative ones.
