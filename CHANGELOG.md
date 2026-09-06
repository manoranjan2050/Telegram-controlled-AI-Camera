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
- **Phase 6 — Send Photo to Telegram**: `telegramp4_telegram_send_photo()`
  (hand-built multipart/form-data upload, streamed without double-buffering the
  JPEG, retries once on failure) and the real `/photo` handler. End-to-end
  behavior still blocked on Phase 5's camera driver.
- **Phase 7 — MicroSD Storage**: `telegramp4_storage` (mount via
  `esp_vfs_fat_sdmmc_mount`, standard subdirectory layout, allow-list filename
  sanitization used by every file-touching command), `/files`, `/storage`,
  `/delete`. SDMMC pins use ESP-IDF's own SoC-default for ESP32-P4, unconfirmed
  against DFRobot's actual board wiring.
- **Phase 8 — Photo Gallery**: `/photos` with `[View][Download][🗑 Delete]`
  buttons per photo; new generic `telegramp4_telegram_send_with_keyboard()` and
  `telegramp4_telegram_send_document()`; one shared `delete_photo_file()` used
  by both `/delete` and the gallery's Delete button.
- **Phase 9 — Video**: `telegramp4_video` abstraction (stub pending hardware
  verification) and `/video [seconds]`, which spawns a dedicated FreeRTOS task
  so a recording never blocks the Telegram poll task for other chats.
- **Phase 10 — Receive Photo From Telegram**: `telegramp4_telegram_download_file()`
  (getFile + size-checked download), photo/voice received callbacks, saves to
  `/sdcard/received/`, `/photo_info`, `/photo_files`. Unauthorized-chat
  filtering moved into `process_update()` so strangers' media is ignored
  before any download is attempted.
- **Phase 11 — Audio Recording**: `telegramp4_audio` abstraction (stub pending
  mic hardware verification) and `/record [seconds]`, same task-per-recording
  pattern as `/video`.
- **Phase 12 — Receive Telegram Voice**: reuses Phase 10's download
  infrastructure (added a `duration_s` field to the shared media-received
  callback) to save incoming voice notes as-is (OGG/Opus) to
  `/sdcard/received/` and acknowledge with duration - no transcoding yet.
- **Phase 13 — Voice Commands**: new modular `telegramp4_stt` component
  (`telegramp4_stt_transcribe()`), OpenAI Whisper as the one concrete
  provider (real HTTPS multipart integration, disabled by default -
  `TELEGRAMP4_STT_ENABLED`), `parse_voice_command()` keyword matching, and
  `telegramp4_telegram_dispatch()` so a recognized voice command runs through
  the same path as typed commands/buttons.

### Fixed
- Partition table (two 2MB OTA slots + nvs/otadata/phy_init) needs >4MB of
  flash; `sdkconfig.defaults` now assumes 8MB (`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`)
  since the default 2MB config failed to build. Actual FireBeetle 2 ESP32-P4
  flash size is still unconfirmed — see docs/hardware.md.

See [CLAUDE.md](CLAUDE.md) for the live progress checklist.
