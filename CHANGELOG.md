# Changelog

All notable changes to this project are documented here.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased] - 2026-09-10 (round 5: web setup portal for first-time provisioning)

### Added
- **First-time setup web dashboard** (`telegramp4_provisioning`). A device
  flashed with no WiFi/Telegram credentials baked in starts a SoftAP
  (`TelegramP4-Setup`) and a web server at `http://192.168.4.1/` with a
  form for WiFi SSID/password, Telegram bot token, and chat ID(s).
  Submitting it saves the values to NVS and reboots into normal operation
  - no `idf.py menuconfig`, rebuild, or serial connection needed. Holding
  the BOOT button (GPIO35) at power-on forces re-entry into setup later.
  `telegramp4_wifi_init()`, `telegramp4_telegram_start()`, and
  `telegramp4_security_configure()` all changed from reading Kconfig
  directly to taking these values as parameters; developers who still bake
  real credentials into `sdkconfig` are unaffected (NVS checked first,
  falls back to Kconfig only if NVS is empty). Verified live: portal
  starts on an unconfigured device, the setup page renders and is
  reachable at 192.168.4.1, and submitting it correctly saves to NVS and
  reboots. See docs/lessons/17-provisioning.md.

## [Unreleased] - 2026-09-10 (real hardware, round 4: SD card fixed - it was never hardware)

### Fixed
- **SD card mount, root-caused and fixed - was never a hardware defect.**
  `sdmmc_init_ocr: send_op_cond (1) returned 0x107` survived four rounds of
  real firmware fixes (correct pins, correct host slot, power-gate polarity
  both directions, hardware pull-ups) and two different physical microSD
  cards (one PC-verified healthy, freshly FAT32-formatted) with zero change
  in behavior. Root-caused by flashing DFRobot's own official MicroPython
  factory test for this exact board
  (wiki.dfrobot.com/dfr1172/docs/22904, identical pins/slot) - with WiFi
  never touched, the card mounted instantly. The SD card (SDMMC host slot
  0) and the ESP32-C6 WiFi link (SDIO, slot 1) share one physical
  SDMMC/SDIO host controller; `app_main()` always started WiFi first,
  leaving shared host state slot 0's own init could never recover from.
  Fixed by reordering `main/app_main.cpp`: camera → SD → WiFi. Confirmed
  live: all three now succeed together in the same boot, first time ever.
- Getting camera + SD + WiFi to all succeed together surfaced a second,
  unrelated bug: `CONFIG_VFS_MAX_COUNT` (default 8) is a fixed-size table
  of *registered VFS backends*, not open files - stdio/eventfd, the
  camera's two `/dev/videoN` device nodes, and the SD FATFS mount filled it
  before lwip could register its socket range when WiFi started, aborting
  with `ESP_ERR_NO_MEM`. Raised to 16 in `sdkconfig.defaults`.
- Lowered `telegramp4_storage`'s FATFS `max_files` from 8 to 3 (the app
  never has more than 1-2 files open at once) while investigating the
  above - kept as a minor cleanup even though it turned out not to be the
  actual fix.

## [Unreleased] - 2026-09-09 (real hardware, round 3: PSRAM fixed, camera works)

### Fixed
- **PSRAM boot-loop, root-caused and fixed.** `CONFIG_SPIRAM=y` was rebooting
  the device before `app_main()` ever ran
  (`assert failed: xTaskCreateStaticPinnedToCore ... xPortCheckValidTCBMem`/
  `xPortcheckValidStackMem`). Root cause, found via a temporary diagnostic
  print inside FreeRTOS's `xTaskCreateStaticPinnedToCore` (reverted after
  use): with PSRAM enabled, the ESP32-P4's internal-SRAM (L2MEM) heap pool
  is fragmented/short enough at boot that plain `MALLOC_CAP_INTERNAL`
  requests (used by ESP-IDF's default idle/timer-task memory providers and
  by `main_task`'s dynamically-allocated stack) sometimes get satisfied out
  of the chip's tiny 8KB TCM region instead of ordinary SRAM - FreeRTOS then
  rejects a TCM-backed static task buffer as invalid. Fixed entirely from
  the project, no ESP-IDF/SDK files modified: `main/freertos_static_mem.c`
  overrides `vApplicationGetIdleTaskMemory`/`vApplicationGetPassiveIdleTaskMemory`/
  `vApplicationGetTimerTaskMemory` via `-Wl,--wrap=` linker flags (added in
  `main/CMakeLists.txt`) so idle/timer task memory always comes from static
  `.bss`, never the heap; `CONFIG_ESP_MAIN_TASK_STACK_SIZE` bumped from 3584
  to 16384 bytes (above TCM's 8KB size) so `main_task`'s stack structurally
  cannot land there either. (An earlier attempt to simply redefine these
  functions without `--wrap` silently failed to link in - both ESP-IDF's and
  the app's copies live in separate static-library archives, and the linker
  resolves the *first* archive member that satisfies the symbol, which is
  always ESP-IDF's own `port_common.c.o` scanned from within `freertos.a`
  itself before `main.a` is even reached.)
- **Real camera photo capture confirmed working end-to-end.** With PSRAM
  fixed, `/dev/video10` (the JPEG M2M encoder device) still failed to open.
  Root cause: `CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE` defaults to
  `n` in the `espressif/esp_video` managed component's own Kconfig - without
  it, the JPEG device is never registered by `esp_video_init()` at all, no
  matter what PSRAM/memory state the board is in. Enabled in
  `sdkconfig.defaults`. Confirmed live: `/photo` now captures real JPEG
  frames (15155 and 38432 bytes observed) and uploads them to Telegram.
- Microphone PDM pins traced from the schematic: PDM_DATA=GPIO9,
  PDM_CLK=GPIO12 (`docs/hardware.md`).

### Added
- **Real audio recording**, using the mic pins above via ESP-IDF's I2S PDM
  RX driver (`driver/i2s_pdm.h`, 16-bit mono, wrapped in a hand-built WAV
  header). `telegramp4_audio_result_t` now carries the WAV bytes directly in
  a heap buffer (mirroring `telegramp4_camera_frame_t`) instead of requiring
  the SD card to be mounted, since SD doesn't reliably work on this board
  yet - a copy is still saved to `/sdcard/audio/` best-effort when it is.
  Confirmed live: `Recorded 320000 bytes of PCM (10s @ 16000Hz)`,
  `telegramp4_audio_record()` returning `ESP_OK`.

## [Unreleased] - 2026-09-07 (real hardware, round 2)

WiFi + Telegram bot + button menu confirmed stable on real hardware
(`@ESP3P4AIbot`). The user provided DFRobot's actual DFR1172 schematic and
identified the installed camera module (Raspberry Pi Camera Module v1.3 /
OV5647 sensor), which unlocked real fixes instead of guesses:

### Added
- **Real OV5647 camera driver** (`telegramp4_camera`) using `espressif/esp_video`
  + `esp_cam_sensor` (V4L2-style capture + JPEG M2M encode). Confirmed via
  schematic: SCCB SCL=GPIO8/SDA=GPIO7, no reset/pwdn/xclk pin exists on this
  board's CSI connector at all. **Sensor detection confirmed live**:
  `ov5647: Detected Camera sensor PID=0x5647`. Full JPEG capture is still
  blocked on the PSRAM/ESP-Hosted issue below.
- **SD card power gate fix**: schematic revealed the MicroSD socket's power is
  switched by a P-MOSFET gated by GPIO45 (`SD1_PWRN`, active-low), never
  driven by earlier firmware - the card had no power at all, which is why it
  timed out even with correct bus pins. `telegramp4_storage_init()` now
  drives GPIO45 low before mounting.
- Confirmed via DFRobot's own wiki spec table + boot log: **16MB flash, 32MB
  in-package PSRAM** (chip variant ESP32-P4NRW32). `sdkconfig.defaults`
  corrected from the earlier 8MB placeholder to `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`.

### Fixed
- `MAX_COMMANDS` in the Telegram command registry was too small (32) for the
  ~30 commands the full app now registers - `/menu` and `/settings` silently
  failed to register (`Command registry full`). Bumped to 64.
- Four FreeRTOS tasks that make HTTPS calls (`telegram_poll_task`,
  `video_record_task`, `audio_record_task`, `motion_task`) had 2-8KB stacks -
  too small once mbedTLS/esp_http_client's own stack usage stacks on top of
  this project's multi-KB local URL buffers. Hit an actual
  "Guru Meditation Error: Stack protection fault" on hardware. Bumped all
  four to 16KB.
- `esp_http_client`'s default internal buffer (512B) was too small for
  longer URL-encoded GET requests (the `/start` menu's inline-keyboard JSON),
  failing with `HTTP_CLIENT: Out of buffer`. Raised `buffer_size`/
  `buffer_size_tx` to 4096 in `telegramp4_telegram.c`.
- A vendored managed-component script
  (`managed_components/espressif__esp_ipa/tools/config/esp_ipa_config.py`)
  naively splits its input file path on whitespace, breaking because this
  project lives under `D:\My Project\...` (a path with a space). Patched
  in place - see CLAUDE.md for why this patch doesn't survive a clean
  managed-components fetch and how to reapply it.

### Known issue (not yet fixed)
- **Enabling `CONFIG_SPIRAM=y` boot-loops the device.** PSRAM itself
  initializes correctly (`Found 32MB PSRAM device`, memory test passes), but
  ESP-Hosted's very early static task creation then hits
  `assert failed: xTaskCreateStaticPinnedToCore ... xPortCheckValidTCBMem`
  before `app_main()` even runs. Root cause not yet identified. PSRAM is
  disabled in `sdkconfig.defaults` for now (commented out, not deleted) so
  WiFi/Telegram/SD stay on the last known-stable configuration. This blocks
  real JPEG capture, since the camera's frame buffers need PSRAM (~1.28MB for
  two 800x800 RAW8 buffers, well over internal L2MEM budget). Next session
  should start here.

## [1.0.0] - 2026-09-06

All 22 build phases implemented and **build-verified** with ESP-IDF v5.4.1
targeting `esp32p4`. **Not yet tested on real hardware** in this development
session — see [docs/lessons/16-final-project.md](docs/lessons/16-final-project.md)
for an honest checklist against the original success criteria, and
[docs/hardware.md](docs/hardware.md) for exactly which subsystems (camera,
video, microphone, AI model, PIR GPIO, display) are still unverified stubs
pending a real board.

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
- **Phase 21 — OTA**: `telegramp4_ota` (`esp_https_ota`, cert-bundle
  validated, opt-in via `TELEGRAMP4_OTA_ENABLED`), `/version`, `/ota <url>`.
  Unlike most recent phases, standard ESP-IDF functionality - not blocked on
  hardware verification.
- **Phase 22 — System Status (final integration)**: full `/status` dashboard
  (moved into `app_main.cpp` so it can see every component), `/diagnostics`
  (honest `N/A` for anything not actually measured - camera FPS/JPEG size,
  Telegram latency), `/reboot` with `[Yes]/[Cancel]` confirmation, `/delete`
  and every Delete button routed through one shared confirmation flow
  (`/delete_confirm <subdir>:<filename>` / `/cancel`), and the final main
  menu matching spec §19 exactly (Photo/Video/AI/Audio/Gallery/Motion/GPIO/
  Status/Settings).

### Fixed
- Partition table (two 2MB OTA slots + nvs/otadata/phy_init) needs >4MB of
  flash; `sdkconfig.defaults` now assumes 8MB (`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`)
  since the default 2MB config failed to build. Actual FireBeetle 2 ESP32-P4
  flash size is still unconfirmed — see docs/hardware.md.
- Local ESP-IDF v5.4 tooling (`confgen`/kconfiglib) silently produced an
  incomplete `sdkconfig` under Python 3.14 (missing even standard options like
  `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM`), breaking the build. Rebuilt the
  ESP-IDF Python virtual environment against Python 3.11 (installed via NuGet,
  since the Windows Installer-based Python installer failed in this sandboxed
  session) - a local dev-environment fix, not a project code change.
- ESP32-P4 has no native WiFi radio - connectivity goes through the onboard
  ESP32-C6 via the `espressif/esp_wifi_remote` managed component instead of
  the classic on-chip `esp_wifi` driver (confirmed from ESP-IDF's own Kconfig
  and official examples). Added the dependency and
  `CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y`.
- `Kconfig.projbuild` and `idf_component.yml` must live in `main/` to be
  auto-discovered by ESP-IDF - a copy at the repo root (as shown in the
  master spec's illustrative file tree) is silently ignored, which meant
  every TelegramP4-specific Kconfig option was undeclared. Moved both files.
- `main/CMakeLists.txt` was missing `esp_timer` as a dependency despite
  `app_main.cpp` including `esp_timer.h` directly.
- `/video` was implemented in Phase 9 but never actually registered as a
  command (caught by an "unused function" build warning) - wired up.
- `TELEGRAMP4_MOTION_ALERT_COOLDOWN_S`'s Kconfig `depends on
  TELEGRAMP4_MOTION_ENABLED` meant its `CONFIG_` macro didn't exist while
  motion detection was disabled (the default), breaking the build for code
  that references the constant unconditionally. Removed the dependency.

## [Unreleased]

### Fixed — found on real hardware (2026-09-07)
First actual flash to a FireBeetle 2 ESP32-P4. WiFi + Telegram bot
(`/start`, button menu, chat-ID whitelist) now confirmed working end-to-end.
- MicroSD mount used `SDMMC_HOST_SLOT_1` (the `SDMMC_HOST_DEFAULT()` default),
  which is the same hardware slot ESP-Hosted uses for the WiFi link to the
  ESP32-C6 over SDIO. This crashed/reset the WiFi transport in a boot loop
  the moment storage init ran. Switched to `SDMMC_HOST_SLOT_0`.
- `telegram_poll_task`, `video_record_task`, `audio_record_task`, and
  `motion_task` all make HTTPS calls but had 2-8KB stacks; real hardware hit
  a "Guru Meditation Error: Stack protection fault" the first time a command
  handler actually ran an HTTPS round trip. Bumped all four to 16KB.
- `esp_http_client`'s default internal buffer (512 bytes) was too small once
  a request URL got long (the inline-keyboard JSON for `/start`'s menu, or a
  long status reply), failing outright with `HTTP_CLIENT: Out of buffer`.
  Set `buffer_size`/`buffer_size_tx` to 4096.

**Still open:** SD card mount still times out (`0x107`) even on slot 0 — the
MicroSD GPIO pins for this specific board are still unconfirmed (current
code uses ESP-IDF's chip-level default pins, not this board's schematic).
Camera driver is still a stub — a camera module has been physically
installed but the sensor/driver work itself hasn't started.

See [CLAUDE.md](CLAUDE.md) for the live progress checklist.
