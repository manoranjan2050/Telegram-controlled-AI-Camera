# Security

TelegramP4 controls a camera and GPIOs from the internet via Telegram. Security is
mandatory, not optional (spec §8). This document is the reference; every phase that
touches these areas must comply.

## Chat ID whitelist

Only chat IDs listed in `TELEGRAM_ALLOWED_CHAT_IDS` (Kconfig) may issue commands or
press buttons. Every command handler and every callback-query handler must check
this before doing anything. Unauthorized senders receive exactly:
```
Access denied.
```
No further information is leaked — not command names, not device state, not error
detail.

## Bot token handling

- Never committed to Git (`sdkconfig` is gitignored).
- Never logged, at any log level.
- Never included in README/docs examples except as the literal placeholder
  `123456789:REPLACE_WITH_YOUR_BOT_TOKEN`.
- If leaked, revoke immediately via BotFather (`/revoke`).

## Transport security

All Telegram Bot API traffic uses HTTPS with certificate validation enabled.
Certificate verification must never be disabled to "make development easier." If an
insecure development-only mode is ever genuinely required for bring-up, it must be
clearly labeled, isolated behind an explicit Kconfig flag defaulting to off, and
called out loudly in logs when active — it must never be the default or silently
shipped.

## File safety

- All filenames derived from Telegram input (received photos/voice, `/delete`
  arguments, gallery callback payloads) are sanitized before touching the
  filesystem. Reject anything containing `..`, absolute paths, or path separators
  that would escape the intended directory (`/sdcard/photos/`,
  `/sdcard/received/`, etc.).
- Enforce a maximum download size for anything fetched via `getFile`.
- Enforce a maximum video/audio recording duration (Kconfig-configurable).
- Enforce a maximum file/storage count per directory to prevent unbounded SD usage.

## Rate limiting

Implement a basic per-chat cooldown on command processing to prevent command
flooding from overwhelming the device or the Telegram API.

## Destructive action confirmation

`/reboot`, `/delete`, and any other irreversible action must prompt for
confirmation before executing:
```
Are you sure?

[Yes, reboot] [Cancel]
```

## Reporting a vulnerability

See [SECURITY.md](../SECURITY.md) at the repo root.
