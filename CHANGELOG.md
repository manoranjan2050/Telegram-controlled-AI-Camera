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

See [CLAUDE.md](CLAUDE.md) for the live progress checklist.
