# Lesson 16 — Final Integrated Project (Phase 22)

## Goal

Assemble the final `/status` dashboard, `/diagnostics`, the confirmed final
menu, and confirmation prompts for every destructive action.

## What changed in this phase

**Full `/status`** (moved from `telegramp4_telegram` into `main/app_main.cpp`
as `handler_full_status()`, since a real dashboard needs every component):
firmware version, WiFi (state/IP/RSSI), an approximate Telegram connectivity
line, camera readiness, SD card readiness + free space, AI enabled state,
heap/PSRAM, and uptime — matching the spec §22 format. No credentials are
ever included.

**`/diagnostics`** (spec §21): free heap, free PSRAM, storage free, WiFi
RSSI, and AI inference time **if an `/ai` run has actually happened this
session** (`s_last_ai_inference_ms`, set inside `handler_ai()`). Camera
FPS/JPEG size and Telegram round-trip latency are reported as `N/A` — nothing
in this firmware measures them yet, and inventing numbers would violate the
project's core rule against fabricating results.

**Confirmations for destructive actions** (spec §7/§8): `send_confirmation()`
sends a `[Yes, ...] [Cancel]` inline keyboard. `/delete` and the gallery/
received-photo Delete buttons all now go through
`request_delete_confirmation()` → `/delete_confirm <subdir>:<filename>` (one
shared handler regardless of which UI path triggered it) or `/cancel`. A new
`/reboot` asks `[Yes, reboot] [Cancel]` before calling `esp_restart()`.

**Final menu** (spec §19): `build_main_menu_json()` in
`telegramp4_telegram.c` now matches the spec's final button list exactly —
Photo, Video, AI, Audio, Gallery, Motion, GPIO, Status, Settings — each
wired to its real handler.

## Code

- [`main/app_main.cpp`](../../main/app_main.cpp) — `handler_full_status`,
  `handler_diagnostics`, `send_confirmation`, `request_delete_confirmation`,
  `handler_delete_confirm`, `handler_cancel`, `handler_reboot(_confirm)`
- [`components/telegramp4_telegram/telegramp4_telegram.c`](../../components/telegramp4_telegram/telegramp4_telegram.c) —
  final `build_main_menu_json()`; the old WiFi-only `/status` was removed in
  favor of the full one above

## Spec §27 success-criteria check-in

Walking through the master spec's final list, honestly:

| # | Criterion | Status |
|---|---|---|
| 1-7 | Buy board, camera, install ESP-IDF, clone, configure WiFi/Telegram, flash | Software side ready; hardware steps are the user's |
| 8-9 | `/start` / `/status` | ✅ implemented, build-verified, **not yet tested on real hardware** |
| 10-11 | Press Photo, receive real image | ⛔ blocked on Phase 5 camera driver verification |
| 12 | Save photos to SD | ✅ implemented (SDMMC pins use ESP-IDF's SoC default, unconfirmed for this board) |
| 13 | Record/receive short video | ⛔ blocked on the same camera verification |
| 14 | Send image to bot | ✅ implemented and build-verified |
| 15-16 | AI detection + results | ⛔ blocked on Phase 14 model verification |
| 17-18 | Arm motion, receive alert | ⛔ blocked on PIR GPIO + camera verification |
| 19 | Learn each subsystem from lessons | ✅ this lesson series |

**Build status:** every phase's code compiles cleanly with ESP-IDF v5.4.1
targeting `esp32p4` (verified in this session, after fixing two real bugs:
`esp_wifi_remote` needed for P4's WiFi, and `Kconfig.projbuild` needing to
live in `main/` — see CHANGELOG). **No phase has been tested on real
hardware** — that requires the actual FireBeetle 2 board, a camera module,
and a PIR sensor, none of which were available in this development session.
See `docs/hardware.md` for the exact list of unverified assumptions to check
first.

## Next steps for whoever picks this up on hardware

1. Flash Phase 0-4 first and confirm WiFi + Telegram commands work at all.
2. Verify the camera sensor/driver (Phase 5) — this unblocks photo, video,
   AI, and motion-alert functionality all at once.
3. Verify SDMMC pins against the board's actual schematic (Phase 7).
4. Verify the microphone interface (Phase 11) and PIR GPIO (Phase 17)
   similarly.
5. Pick and integrate a real AI model (Phase 14) once the camera works.
