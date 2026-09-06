# CLAUDE.md — TelegramP4 Project Instructions

This file is read automatically by Claude Code at the start of every session in this
repository. It is the condensed, operational version of
[TelegramP4_Master_Project_Spec.md](TelegramP4_Master_Project_Spec.md) (the full spec —
read that file if anything here is ambiguous).

## What this project is

**TelegramP4** — control an ESP32-P4 AI camera and IoT device from Telegram.
A community-friendly, incremental ESP-IDF learning platform, not a one-shot camera demo.

Target hardware: **DFRobot FireBeetle 2 ESP32-P4 AI Vision Board**
(ESP32-P4 main SoC + ESP32-C6 connectivity co-processor, MIPI-CSI camera, MicroSD,
onboard mic, optional MIPI-DSI display, Wi-Fi via C6).
Product page: https://www.dfrobot.com/product-2915.html

## Current status

Track progress here so a new session knows where to resume.

- [x] Phase 0 — Project Bootstrap (code written; build verification pending ESP-IDF install)
- [x] Phase 1 — Wi-Fi (code written; build verification pending ESP-IDF install)
- [x] Phase 2 — Telegram Basic (code written; build verification pending ESP-IDF install)
- [ ] Phase 3 — Telegram Command Framework
- [ ] Phase 4 — Telegram Inline Buttons
- [ ] Phase 5 — Camera
- [ ] Phase 6 — Send Photo to Telegram
- [ ] Phase 7 — MicroSD Storage
- [ ] Phase 8 — Photo Gallery
- [ ] Phase 9 — Video
- [ ] Phase 10 — Receive Photo From Telegram
- [ ] Phase 11 — Audio Recording
- [ ] Phase 12 — Receive Telegram Voice
- [ ] Phase 13 — Voice Commands
- [ ] Phase 14 — AI Vision
- [ ] Phase 15 — Telegram AI Workflow
- [ ] Phase 16 — AI on Telegram Photos
- [ ] Phase 17 — Motion Detection
- [ ] Phase 18 — Motion AI Alert
- [ ] Phase 19 — GPIO / IoT Control
- [ ] Phase 20 — Display
- [ ] Phase 21 — OTA
- [ ] Phase 22 — System Status

Detailed, ready-to-run per-phase prompts live in **[docs/PHASES.md](docs/PHASES.md)**.
Work through them in order — do not skip ahead. Update the checklist above after each
phase is verified.

## Non-negotiable rules (see spec §22, §24 for full list)

1. **Incremental only.** Never build more than the current phase. Every phase must
   compile (`idf.py build`) before moving to the next.
2. **No monolith.** Never put Telegram, camera, AI, and SD logic in one file. Respect
   the component boundaries in §5 of the spec.
3. **No secrets in Git.** Wi-Fi password, Telegram bot token, and allowed chat IDs are
   configured via Kconfig / `sdkconfig` (local, gitignored) — never hardcoded, never
   logged, never in README examples. Placeholder format:
   `123456789:REPLACE_WITH_YOUR_BOT_TOKEN`.
4. **Don't invent hardware capabilities.** Verify FireBeetle 2 ESP32-P4 GPIO/camera/
   audio/display details against current DFRobot/Espressif docs before writing
   hardware-specific code. If unverified, say so — don't guess pin numbers.
5. **Prefer official Espressif components** (esp32-camera, esp-video, esp-sr, esp-dl /
   ESP-DL, esp_https_ota, etc.) over hand-rolled low-level replacements.
6. **TLS stays on.** Telegram traffic must be HTTPS with certificate validation.
   Never ship with certificate verification disabled.
7. **No fabricated test results.** Never claim a camera/AI/video/hardware test passed
   without actually running it on hardware. If hardware isn't available in the current
   environment, state plainly: *"Build verified; hardware test required."*
8. **Memory discipline.** Use PSRAM / `heap_caps_malloc` for camera/JPEG/video/AI
   buffers, free everything after use, and log free heap/PSRAM after large operations.
9. **Don't block.** No blocking delays on the Telegram task; long operations (video,
   AI inference) must not freeze command handling — use FreeRTOS tasks/queues.
10. **Security always on.** Chat-ID whitelist enforced on every command, filename
    sanitization on every file operation (no `../` traversal), confirmation prompts for
    destructive actions (`/reboot`, `/delete`).

## Repository layout

See spec §5 for the full target tree (`main/`, `components/telegramp4_*`, `docs/`,
`examples/`, `partitions/`, etc.). Component names are fixed:
`telegramp4_telegram`, `telegramp4_wifi`, `telegramp4_camera`, `telegramp4_video`,
`telegramp4_audio`, `telegramp4_storage`, `telegramp4_ai`, `telegramp4_motion`,
`telegramp4_gpio`, `telegramp4_display`, `telegramp4_config`, `telegramp4_security`,
`telegramp4_ota`, `telegramp4_system`, `telegramp4_board`.

## Workflow for every phase (spec §23, §29)

1. Inspect the existing project state and this checklist.
2. Read the matching phase prompt in `docs/PHASES.md`.
3. Implement only that phase.
4. `idf.py build` — fix errors until it compiles.
5. Explain what changed, in plain language.
6. Give exact flash/monitor/test commands and expected Telegram output.
7. Update/create the matching lesson doc under `docs/lessons/`.
8. Update the checklist in this file.
9. **Stop at the phase boundary** and wait for confirmation before continuing, unless
   the user has explicitly asked to proceed through multiple phases.

## Environment notes

- Development machine is Windows 11 (PowerShell). ESP-IDF commands (`idf.py`) must be
  run from an ESP-IDF environment (`export.ps1`/`export.bat` sourced, or ESP-IDF
  PowerShell shortcut). Verify this is set up before Phase 0.
- No hardware-in-the-loop testing from this chat session unless the user explicitly
  connects and flashes a board and reports back results.
