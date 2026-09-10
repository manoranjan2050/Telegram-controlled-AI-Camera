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

**Real hardware test log (2026-09-11, FireBeetle 2 ESP32-P4 DFR1172):**
- ✅ **Fixed dark/underexposed photos and video.** Root cause:
  `CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER` (default off) gates
  Espressif's entire closed-loop 3A auto-exposure/auto-gain/auto-white-balance
  controller - with it off, the camera ran on the sensor's raw power-on
  defaults with zero active exposure control. A first attempt (nudging the
  sensor's AE-target register via `VIDIOC_S_CTRL`) failed silently because
  esp_video only implements the extended controls API
  (`VIDIOC_S_EXT_CTRLS`), not the simple one. Confirmed visually via a
  before/after photo dumped over serial: near-total black before, clearly
  readable "RADEON" text and RGB lighting detail after, same dim room. See
  docs/lessons/05-camera.md.
- ✅ **Fixed quiet audio recordings.** `I2S_PDM_RX_SLOT_DEFAULT_CONFIG()`'s
  `amplify_num` defaulted to 1 (the driver's minimum, range 1-15) - raised
  to 8 in `telegramp4_audio.c`. See docs/lessons/10-audio.md.
- ✅ **Real H.264 video recording works.** `espressif/esp_video` exposes
  the P4's hardware H.264 encoder as a V4L2 M2M device (`/dev/video11`,
  same pattern as the JPEG path) - needed `CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE=y`
  (defaults off) and configuring the CSI capture device for packed YUV420
  output (the exact format the encoder needs, confirmed by reading
  `esp_video_h264_device.c`'s source - no software pixel conversion
  needed). Photo and video modes are mutually exclusive (one capture
  engine) - `telegramp4_camera_start/stop_video_mode()` handle the
  teardown/restore automatically. Confirmed live: 37 frames, 36816 bytes
  of H.264, 5s recording. Delivered as `video.h264` (raw Annex-B stream,
  not MP4 - VLC/ffplay play it directly). See docs/lessons/08-video.md.
- ✅ **Web setup portal added.** First-time users flash once with no
  credentials baked in; the device starts a SoftAP (`TelegramP4-Setup`) +
  web page at `http://192.168.4.1/` for entering WiFi + Telegram bot
  token/chat ID, saves to NVS, and reboots into normal operation. Hold
  BOOT (GPIO35) at power-on to re-enter setup later. Developers who prefer
  baking real values into `sdkconfig` are unaffected (NVS checked first,
  falls back to Kconfig). See docs/lessons/17-provisioning.md.
- ✅ WiFi connects reliably (`esp_wifi_remote` over SDIO to the C6)
- ✅ Telegram bot connects and responds (`@ESP3P4AIbot`), button menu works
- ✅ **PSRAM boot-loop FIXED.** Root cause: with `CONFIG_SPIRAM=y`, the
  internal-SRAM (L2MEM) heap pool is fragmented/short enough by early boot
  that ESP-IDF's default idle/timer-task memory providers and `main_task`'s
  dynamic stack (all allocated via `pvPortMalloc`/`MALLOC_CAP_INTERNAL`)
  sometimes get satisfied out of the ESP32-P4's tiny 8KB TCM region instead
  of ordinary SRAM — FreeRTOS then rejects a TCM-backed static task
  buffer as invalid and asserts/reboots. Fixed entirely from the project
  (no ESP-IDF/SDK files modified) via `main/freertos_static_mem.c` +
  `main/CMakeLists.txt`'s `-Wl,--wrap=` linker flags (idle/timer task memory
  now comes from static `.bss`, never the heap) plus bumping
  `CONFIG_ESP_MAIN_TASK_STACK_SIZE` to 16384 (above TCM's 8KB, so that
  allocation structurally can't land there either). See the header comment
  in `main/freertos_static_mem.c` for the full diagnostic story.
- ✅ **Real camera photo capture WORKS.** Sensor detected
  (`ov5647: Detected Camera sensor PID=0x5647`), JPEG M2M pipeline opens and
  streams (`Camera initialized (800x800)`), and `/photo` produces real JPEG
  frames (confirmed live: 15155 and 38432 byte captures) uploaded to
  Telegram. The missing piece was `CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE`
  (defaults to `n` in the `espressif/esp_video` managed component's own
  Kconfig) — without it, `/dev/video10` (`ESP_VIDEO_JPEG_DEVICE_NAME`) is
  never registered at all, regardless of PSRAM.
- ✅ **SD card mount FIXED — was never a hardware defect.** Days of
  `send_op_cond (1) returned 0x107` failures survived pin fixes, host-slot
  separation, power-gate polarity (both directions), and pull-ups — all
  real fixes, none of which touched the actual bug. Root cause: WiFi's SDIO
  init (slot 1) and the SD card (slot 0) share one physical SDMMC/SDIO host
  controller, and WiFi always started first, leaving shared host state SD's
  own init couldn't recover from. Proved by flashing DFRobot's own official
  MicroPython factory test for this board (identical pins) with WiFi never
  touched — the card mounted instantly. Fixed by reordering `app_main()`:
  camera → SD → WiFi (see `main/app_main.cpp`). Getting all three to
  actually succeed together then exposed `CONFIG_VFS_MAX_COUNT` (default 8)
  filling up before lwip's socket range could register — raised to 16.
  See docs/hardware.md and docs/lessons/07-microsd.md for the full story.
- ✅ **Real audio recording works.** Mic pins confirmed from the schematic
  (PDM_DATA=GPIO9, PDM_CLK=GPIO12). `telegramp4_audio_record()` uses
  ESP-IDF's I2S PDM RX driver, returns a WAV file in a heap buffer (same
  pattern as camera frames, since SD isn't reliable enough to depend on).
  Confirmed live: `Recorded 320000 bytes of PCM (10s @ 16000Hz)`.
- All core features (WiFi, Telegram, camera, video, audio, SD, web setup)
  are now confirmed working on real hardware. Remaining opt-in features
  (AI detection, motion alerts, voice commands, display) are implemented
  and build-clean but need per-user hardware (API key, PIR sensor, display
  panel) to verify - see the feature table in README.md.

- [x] Phase 0 — Project Bootstrap (code written; build verification pending ESP-IDF install)
- [x] Phase 1 — Wi-Fi (code written; build verification pending ESP-IDF install)
- [x] Phase 2 — Telegram Basic (code written; build verification pending ESP-IDF install)
- [x] Phase 3 — Telegram Command Framework (code written; build verification pending ESP-IDF install)
- [x] Phase 4 — Telegram Inline Buttons (code written; build verification pending ESP-IDF install)
- [x] Phase 5 — Camera (abstraction + Kconfig written; actual sensor driver is a stub — hardware verification required, see docs/lessons/05-camera.md)
- [x] Phase 6 — Send Photo to Telegram (multipart upload implemented; end-to-end blocked on Phase 5 camera hardware verification)
- [x] Phase 7 — MicroSD Storage (mount logic + sanitization implemented using ESP-IDF's SoC-default P4 SDMMC pins; unconfirmed against DFRobot's actual wiring)
- [x] Phase 8 — Photo Gallery (code written; build verification in progress)
- [x] Phase 9 — Video (task-based /video handler + abstraction written; encoder is a stub pending Phase 5 hardware verification)
- [x] Phase 10 — Receive Photo From Telegram (getFile download, size-capped, /photo_info /photo_files)
- [x] Phase 11 — Audio Recording (task-based /record handler + abstraction written; mic driver is a stub pending hardware verification)
- [x] Phase 12 — Receive Telegram Voice (download + save + acknowledge with duration)
- [x] Phase 13 — Voice Commands (modular telegramp4_stt + OpenAI Whisper provider, disabled by default; real but untested against a live API key)
- [x] Phase 14 — AI Vision (abstraction written; model/framework choice unverified, stub pending hardware)
- [x] Phase 15 — Telegram AI Workflow (/ai wired to camera+AI, honest error until Phase 14 lands)
- [x] Phase 16 — AI on Telegram Photos (Detect/Save/Delete buttons on received images)
- [x] Phase 17 — Motion Detection (ISR->task pattern, /arm /disarm /motion; PIR GPIO unverified, defaults unset)
- [x] Phase 18 — Motion AI Alert (cooldown + broadcast to all whitelisted chats; honest fallback when AI unverified)
- [x] Phase 19 — GPIO / IoT Control (whitelist-enforced /gpio + inline toggle buttons)
- [x] Phase 20 — Display (compile-time optional, stub pending panel/driver verification)
- [x] Phase 21 — OTA (esp_https_ota, /version + /ota <url>, opt-in via Kconfig, not hardware-uncertain)
- [x] Phase 22 — System Status (final integration: full /status, /diagnostics, /reboot + /delete confirmations, final menu) — build-verified 1.0.0, hardware testing still required for all phase 5+ hardware-dependent features

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

## Deviations from the spec's illustrative file tree

The master spec (§5) shows `Kconfig.projbuild` and `idf_component.yml` at the
repository root. In practice ESP-IDF only auto-discovers `Kconfig.projbuild`
and `idf_component.yml` inside a *component* directory (`main/` counts as
one) — a copy at the repo root next to the top-level `CMakeLists.txt` is
silently ignored, which caused a real "undeclared CONFIG_ option" build
failure. Both files now live in `main/` instead. If you're looking for the
project's Kconfig options or component-manager manifest, check `main/`, not
the repo root.

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

- Development machine is Windows 11 (PowerShell). ESP-IDF v5.4.1 is installed at
  `C:\esp\esp-idf`. The system's default Python (3.14) is NOT compatible with
  ESP-IDF v5.4's Kconfig tooling (`confgen`/kconfiglib silently produces an
  incomplete `sdkconfig` under it — see CHANGELOG). A working Python 3.11 was
  installed via NuGet (the Windows Installer-based installer failed in this
  sandboxed session) at `C:\esp\nuget_python\python.3.11.9\tools\python.exe`,
  and the ESP-IDF Python venv was rebuilt against it
  (`~/.espressif/python_env/idf5.4_py3.11_env`). **To build:** prepend that
  Python to `PATH` before sourcing `export.ps1`, e.g.:
  ```powershell
  $env:PATH = "C:\esp\nuget_python\python.3.11.9\tools;" + $env:PATH
  . C:\esp\esp-idf\export.ps1
  idf.py build
  ```
  Skipping the `PATH` prepend causes `export.ps1` to pick the system Python 3.14
  again and the build to fail with "undeclared CONFIG_ option" errors.
- **A vendored managed-component script needed a local patch.** Once the
  camera (Phase 5) pulled in `espressif/esp_video`/`esp_cam_sensor`, the build
  started failing with `json.decoder.JSONDecodeError` inside
  `managed_components/espressif__esp_ipa/tools/config/esp_ipa_config.py`. Root
  cause: that script does `input.split()` on the `-i` file path with no
  quoting awareness, and our project lives under `D:\My Project\...` — a path
  containing a space — so it splits the single path into garbage fragments
  and fails to open a real file. A Windows directory junction workaround
  (`C:\esp\proj` → this folder) did **not** help, because something in the
  toolchain canonicalizes back to the real path anyway. The actual fix:
  `managed_components/espressif__esp_ipa/tools/config/esp_ipa_config.py`
  is patched in place (a few lines, clearly commented "Patched
  2026-09-07") to use the string as-is when it's already a single existing
  file, instead of blindly splitting on whitespace. **This patch lives under
  `managed_components/`, which is gitignored and gets wiped by
  `idf.py fullclean` / a fresh `idf.py build` on a machine without it
  already fetched** — if the build starts failing with this exact
  `JSONDecodeError` again after a clean managed-components fetch, re-apply
  the same one-line fix (or move/rename the project so its full path has no
  spaces, which avoids needing the patch at all).
- No hardware-in-the-loop testing from this chat session unless the user explicitly
  connects and flashes a board and reports back results. Every phase's code has
  been build-verified (compiles + links) as of Phase 22, but nothing has run on
  an actual FireBeetle 2 ESP32-P4 board yet.
