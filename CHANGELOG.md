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
- **Phase 14 — AI Vision**: `telegramp4_ai` abstraction (`AI` Kconfig submenu,
  `TELEGRAMP4_AI_ENABLED` default off) - stub pending model/framework choice
  and hardware verification.
- **Phase 15 — Telegram AI Workflow**: `/ai` (capture -> inference -> photo +
  formatted result), reusing the same handler for the "🤖 AI Detect" menu
  button; reports a clean error while AI is disabled/unverified.
- **Phase 16 — AI on Telegram Photos**: after a photo is received, offers
  `[Detect Objects][Save][Delete]` buttons (`/received_ai`, `/received_save`,
  `/received_delete`), reusing the same AI/sanitization/delete paths as
  everywhere else.
- **Phase 17/18 — Motion Detection + AI Alert**: `telegramp4_motion`
  (GPIO ISR -> task, `/arm`/`/disarm`/`/motion`), full motion -> capture ->
  AI-filter -> Telegram-broadcast pipeline with a configurable cooldown;
  new `telegramp4_security_get_allowed_ids()` for broadcasting to every
  whitelisted chat. PIR GPIO defaults unset pending hardware verification.

- **Phase 19 — GPIO / IoT Control**: `telegramp4_gpio` (whitelist-only pin
  control), `/gpio` (list + toggle buttons), `/gpio <pin> on|off`.
- **Phase 20 — Display**: `telegramp4_display` (compile-time optional,
  `TELEGRAMP4_DISPLAY_ENABLED` default off, stub pending panel/driver
  verification), periodic status refresh task spawned only when a display is
  actually enabled/initialized.

### Fixed
- `/video` was implemented in Phase 9 but never actually registered as a
  command (caught by an "unused function" build warning once a real build
  finally succeeded) - now wired up in `app_main()`.
- `TELEGRAMP4_MOTION_ALERT_COOLDOWN_S`'s Kconfig `depends on
  TELEGRAMP4_MOTION_ENABLED` meant the option (and its CONFIG_ macro) didn't
  exist at all while motion detection was disabled (the default), breaking
  the build for code that references the constant unconditionally. Removed
  the dependency - it's just an int, harmless to have defined even when
  motion detection is off.

### Fixed
- Local ESP-IDF v5.4 tooling (`confgen`/kconfiglib) silently produced an
  incomplete `sdkconfig` under Python 3.14 (missing even standard options like
  `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM`), breaking the build. Rebuilt the
  ESP-IDF Python virtual environment against Python 3.11 (installed via NuGet,
  since the Windows Installer-based Python installer failed in this sandboxed
  session) - this is a local dev-environment fix, not a project code change.

### Fixed
- Partition table (two 2MB OTA slots + nvs/otadata/phy_init) needs >4MB of
  flash; `sdkconfig.defaults` now assumes 8MB (`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`)
  since the default 2MB config failed to build. Actual FireBeetle 2 ESP32-P4
  flash size is still unconfirmed — see docs/hardware.md.

See [CLAUDE.md](CLAUDE.md) for the live progress checklist.
