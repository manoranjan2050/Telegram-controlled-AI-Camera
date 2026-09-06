# Telegram Bot Setup Guide

This guide gets you a bot token and your chat ID so TelegramP4 can talk to you and
only you.

> Never put a real bot token in documentation, code comments, commit messages, or
> screenshots you share publicly. Treat it like a password — anyone with the token
> can control your bot.

## 1. Open Telegram

Use the Telegram app (mobile or desktop) or web.telegram.org.

## 2. Find BotFather

Search for `@BotFather` (verified, blue checkmark) and start a chat.

## 3. Create a bot

Send:
```
/newbot
```
Follow the prompts for a display name and a unique username (must end in `bot`,
e.g. `MyTelegramP4Bot`).

## 4. Obtain the token

BotFather replies with an HTTP API token that looks like:
```
123456789:REPLACE_WITH_YOUR_BOT_TOKEN
```
(the example above is a placeholder shape only — yours will be a real value).

## 5. Configure the token securely

Run:
```bash
idf.py menuconfig
```
Navigate to `TelegramP4 Configuration → Telegram → Bot Token` and paste your token
there. This is written to your local `sdkconfig`, which is listed in `.gitignore`
and must never be committed.

Do **not**:
- paste the token into a GitHub issue, PR, or commit
- hardcode it in any `.c`/`.cpp`/`.h` file
- log it (the firmware is written to never log it — don't add logging that would)

If you ever expose a token accidentally, revoke it immediately via BotFather:
`/revoke`.

## 6. Obtain your chat ID

Easiest method:
1. Send any message to your new bot.
2. Visit `https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates` in a browser
   (replace `<YOUR_TOKEN>` with your real token, and only do this locally/privately
   — this URL contains your token).
3. Find `"chat":{"id": ... }` in the JSON response — that number is your chat ID.

## 7. Add the chat ID to configuration

In `idf.py menuconfig`, under `TelegramP4 Configuration → Telegram → Allowed Chat
IDs`, add your chat ID (comma-separated if adding more than one). Only chat IDs
listed here can control the device — everyone else gets `Access denied.`

## 8. Test `/start`

Flash and run the firmware, then send `/start` to your bot in Telegram. You should
see a welcome message. If nothing happens, see
[troubleshooting.md](troubleshooting.md).
