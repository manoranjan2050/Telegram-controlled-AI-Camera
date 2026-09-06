# Changelog

All notable changes to this project are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Added
- Project documentation scaffolding: README, CLAUDE.md, docs/ (architecture,
  hardware, telegram-bot-setup, configuration, security, memory-and-performance,
  troubleshooting, getting-started), CONTRIBUTING, SECURITY, CODE_OF_CONDUCT.
- Phase-by-phase build plan (`docs/PHASES.md`) covering Phase 0 through Phase 22.
- **Phase 0 — Project Bootstrap**: ESP-IDF project skeleton (`main/app_main.cpp`,
  `components/telegramp4_board`), Kconfig menu shell, two-OTA-slot partition table,
  and Lesson 01.
- **Phase 1 — WiFi**: `telegramp4_wifi` component (STA mode, Kconfig-based
  credentials, auto-reconnect, IP/RSSI query), wired into `app_main`, and Lesson 02.
- **Phase 2 — Telegram Basic**: `telegramp4_telegram` component (HTTPS Bot API
  client with cert-bundle TLS validation, `getMe`/`getUpdates` long polling,
  `/start` `/help` `/status`), Telegram Kconfig submenu, and Lesson 03.
- **Phase 3 — Telegram Command Framework**: reusable command registry
  (`telegramp4_telegram_register_command`), new `telegramp4_security` component
  enforcing a chat-ID whitelist before every command runs, `/photo` stub, and the
  `Allowed Telegram Chat IDs` Kconfig option.
- **Phase 4 — Telegram Inline Buttons**: main menu (`telegramp4_telegram_send_menu`)
  sent on `/start`/`/menu`, callback-query handling routed through the same
  `dispatch_command()` used by typed commands, and stub handlers for every
  not-yet-built menu item (`/video`, `/photos`, `/record`, `/ai`, `/storage`,
  `/settings`).
- **Phase 5 — Camera**: `telegramp4_camera` abstraction and Camera Kconfig
  submenu, `/photo_test` command. Actual sensor bring-up is an honest stub
  pending hardware verification (web docs were unreachable while writing this
  phase) — see docs/lessons/05-camera.md for the exact follow-up steps.

See [CLAUDE.md](CLAUDE.md) for the live progress checklist.
