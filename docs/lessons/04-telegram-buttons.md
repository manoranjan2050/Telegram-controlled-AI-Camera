# Lesson 04 — Telegram Inline Buttons

## Goal

Add the main TelegramP4 button menu, and make sure pressing a button runs the
exact same code as typing the equivalent command.

## What you learn

- Telegram inline keyboards (`reply_markup`) and callback queries
- Why button and command handling must never be duplicated — one registry, two
  entry points
- Acknowledging callback queries with `answerCallbackQuery` so the Telegram
  client's loading spinner clears

## Hardware

Same as Lesson 03.

## Wiring

None.

## Code

- [`components/telegramp4_telegram/telegramp4_telegram.c`](../../components/telegramp4_telegram/telegramp4_telegram.c):
  - `build_main_menu_json()` / `telegramp4_telegram_send_menu()` — builds and sends
    the inline keyboard
  - `handle_callback_query()` — extracts `callback_data` and the sender's chat ID,
    then calls **`dispatch_command()`** — the identical function a typed command
    goes through
  - `answer_callback_query()` — calls `answerCallbackQuery` after dispatch

## How it works

The menu is eight buttons, one per row, each with a `callback_data` equal to a
command name (`/photo`, `/video`, `/photos`, `/record`, `/ai`, `/status`,
`/storage`, `/settings`):

```
┌────────────────────────────┐
│       TelegramP4           │
├────────────────────────────┤
│ 📸 Take Photo              │
│ 🎥 Record Video            │
│ 🖼 Last Photo              │
│ 🎙 Record Audio            │
│ 🤖 AI Detect               │
│ 📊 Status                  │
│ 💾 SD Card                 │
│ ⚙ Settings                 │
└────────────────────────────┘
```

`/start` now sends the welcome text *and* the menu; a new `/menu` command
re-sends just the menu. When a button is pressed, Telegram sends an update with a
`callback_query` object instead of a `message`. `process_update()` checks for
`callback_query` first and routes it to `handle_callback_query()`, which pulls out
`data` (the callback_data string) and the chat ID, then calls
**`dispatch_command(chat_id, data)`** — the same function `process_update()` calls
for a typed `/photo`. This is what guarantees the button and the command can never
drift apart: there is exactly one dispatch path, and exactly one place the
chat-ID whitelist is checked.

Features that aren't built yet (Photo, Video, Audio, AI, SD Card, Settings) still
reply "Not implemented yet - see Phase N" — pressing the button behaves exactly
like typing the command, because it *is* the command.

After dispatch, `answer_callback_query()` calls Telegram's `answerCallbackQuery`
so the button's loading spinner clears — this happens whether or not the chat was
authorized, since it's just UI feedback, not information disclosure.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
In Telegram: send `/start`, confirm the menu appears, tap "📊 Status" and confirm
it returns the same text `/status` would.

## Expected output

Serial monitor when a button is tapped:
```
I (xxx) TAG_TELEGRAM: Received callback from chat 123456789: /status
```
Telegram: the same status message you'd get from typing `/status`.

## Common errors

- Menu doesn't appear — check the `reply_markup` JSON is valid (log it at DEBUG
  level temporarily) and that it's URL-encoded correctly.
- Button spinner never clears — confirm `answerCallbackQuery` is actually being
  called and getting HTTP 200.
- See [docs/troubleshooting.md](../troubleshooting.md).

## Challenge

Add a confirmation step for a placeholder "Reboot" button (not in the current
command list) using a two-button `[Yes] [Cancel]` inline keyboard — this is the
same pattern Phase 22 needs for the real `/reboot` and `/delete` confirmations.

## Next lesson

[Lesson 05 — Camera capture](05-camera.md)
