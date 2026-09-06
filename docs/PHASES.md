# TelegramP4 — Phase-by-Phase Build Prompts

This file contains one **ready-to-paste prompt per phase**. Each prompt is
self-contained: paste it into Claude Code (in this repo) and it will implement
exactly that phase, and only that phase, then stop.

Rules for every prompt (do not repeat these to Claude each time — they're already in
[CLAUDE.md](../CLAUDE.md), which is auto-loaded):

- Keep the project buildable after the phase.
- Do not implement anything beyond the phase's scope.
- Verify with `idf.py build`; if hardware testing isn't possible in the current
  session, say "Build verified; hardware test required" instead of pretending.
- End the turn by updating the checklist in `CLAUDE.md` and writing/updating the
  matching lesson under `docs/lessons/`.

Work through phases **in order**. Do not start phase N+1 until phase N is confirmed
working (build, and hardware where applicable).

---

## Phase 0 — Project Bootstrap

> Set up a clean ESP-IDF project for TelegramP4 targeting the DFRobot FireBeetle 2
> ESP32-P4 AI Vision Board (https://www.dfrobot.com/product-2915.html).
>
> Before writing code:
> 1. Run `idf.py --version` and `idf.py --list-targets` to confirm ESP-IDF is
>    installed and supports `esp32p4`. Report the results.
> 2. Look up current DFRobot/Espressif documentation for this board (chip revision,
>    flash/PSRAM size, camera connector type, MicroSD pin mapping, mic interface,
>    display connector, and any DFRobot-specific sdkconfig requirements). Summarize
>    what you find and flag anything you cannot verify.
>
> Then create the project skeleton per the repository structure in
> `TelegramP4_Master_Project_Spec.md` §5: `main/app_main.cpp`, root `CMakeLists.txt`,
> `sdkconfig.defaults`, `Kconfig.projbuild`, `partitions/partitions.csv`,
> `idf_component.yml`, `.gitignore` (must ignore `sdkconfig`, `build/`,
> `managed_components/`, and any local secrets file), and an empty
> `components/telegramp4_board/` component for board-specific pin definitions.
>
> `app_main.cpp` should just set the target, initialize logging/NVS, and print:
> ```
> TelegramP4 starting...
> Board: FireBeetle 2 ESP32-P4
> Firmware version: 0.1.0
> ```
>
> Run `idf.py set-target esp32p4` and `idf.py build`. Fix any errors. Report exact
> `idf.py flash` / `idf.py monitor` commands and the expected serial output. Do not
> implement Wi-Fi, Telegram, or camera yet.

---

## Phase 1 — Wi-Fi

> Implement the `telegramp4_wifi` component (STA mode only).
>
> Requirements: SSID/password read from Kconfig (never hardcoded), automatic
> reconnect on disconnect, Wi-Fi event handling via the ESP-IDF event loop, IP address
> logging on connect, a connection timeout, and an internal event/state so other
> modules can query "connected / disconnected / connecting". Never log the Wi-Fi
> password.
>
> Wire it into `app_main.cpp` so boot logs show:
> ```
> WiFi connecting...
> WiFi connected
> IP: 192.168.x.x
> ```
>
> Build with `idf.py build`. If you have hardware available, flash and confirm the
> log output; otherwise state "Build verified; hardware test required."
>
> Do not implement Telegram yet.

---

## Phase 2 — Telegram Basic

> Implement a minimal Telegram Bot API HTTPS client (new component
> `telegramp4_telegram`) using ESP-IDF's `esp_http_client` with TLS certificate
> validation enabled (do not disable cert verification).
>
> Add Kconfig options for the bot token (placeholder
> `123456789:REPLACE_WITH_YOUR_BOT_TOKEN`, never a real token) and never print the
> token in logs. Implement `getMe` (to confirm the bot is reachable) and long-polling
> `getUpdates`.
>
> Handle exactly these commands, replying in plain text: `/start`, `/help`,
> `/status` (WiFi state, IP, uptime, free heap, PSRAM availability — see spec §2
> example format). No inline buttons, no command framework/registry yet — a simple
> if/else or switch on the raw command text is fine for this phase; the reusable
> framework comes in Phase 3.
>
> Build, and document exactly how to create a bot via BotFather and where to put the
> token (this becomes the basis for `docs/telegram-bot-setup.md`). State whether
> hardware/live-Telegram testing was actually performed or is still required.

---

## Phase 3 — Telegram Command Framework

> Replace the ad-hoc command handling from Phase 2 with a reusable command registry:
> `register_command("/name", handler)` supporting `/command`, `/command arg`, and
> `/command arg1 arg2`. Unknown commands reply:
> ```
> Unknown command.
>
> Use /help to see available commands.
> ```
>
> Add a `telegramp4_security` component implementing a chat-ID whitelist read from
> Kconfig (`TELEGRAM_ALLOWED_CHAT_IDS`, comma-separated, supports multiple IDs).
> Every command handler must check authorization before running. Unauthorized users
> get exactly `Access denied.` and nothing else — do not leak device state, command
> lists, or error details to them.
>
> Re-register `/start`, `/help`, `/status` through the new framework (keep them
> working identically). Add a stub `/photo` handler that just replies
> "Not implemented yet" (real camera support is Phase 5/6) — this proves the registry
> supports future commands without changing the dispatcher.
>
> Build and verify `/help`, an unknown command, and (if you have a second Telegram
> account/chat ID available) the unauthorized-user path.

---

## Phase 4 — Telegram Inline Buttons

> Add inline-keyboard support to `telegramp4_telegram` (Telegram `sendMessage` with
> `reply_markup`, and callback-query handling via `getUpdates`).
>
> Build the main menu (spec §4):
> ```
> ┌────────────────────────────┐
> │       TelegramP4           │
> ├────────────────────────────┤
> │ 📸 Take Photo              │
> │ 🎥 Record Video            │
> │ 🖼 Last Photo              │
> │ 🎙 Record Audio            │
> │ 🤖 AI Detect               │
> │ 📊 Status                  │
> │ 💾 SD Card                 │
> │ ⚙ Settings                 │
> └────────────────────────────┘
> ```
> shown on `/start` and via a new `/menu` command.
>
> Critical constraint: **button callbacks must call the exact same internal handler
> functions as the text commands** (e.g. the "📊 Status" button calls the same
> function `/status` calls). Do not duplicate business logic between the callback
> handler and the command handler.
>
> For buttons whose feature isn't built yet (Photo, Video, Audio, AI, SD Card),
> reply "Not implemented yet — see Phase N" referencing the correct future phase.
> Apply the chat-ID whitelist to callback queries too, not just text commands.
>
> Build and verify the menu renders and buttons round-trip to the right handler.

---

## Phase 5 — Camera

> Implement the `telegramp4_camera` component. First, verify (via current Espressif/
> DFRobot docs) which camera sensor ships with or is recommended for the FireBeetle 2
> ESP32-P4 AI Vision Board, and which official component supports it on ESP32-P4
> (e.g. `esp32-camera` vs the newer `esp-video`/V4L2-based MIPI-CSI stack — ESP32-P4
> camera support differs from the classic ESP32 DVP camera driver, confirm which
> applies here before writing init code). Report what you find before implementing.
>
> Expose:
> ```cpp
> camera_init();
> camera_deinit();
> camera_capture();
> camera_release_frame();
> camera_get_status();
> ```
> Add Kconfig for resolution and JPEG quality. Add a `/photo_test` command (through
> the Phase 3 registry) that captures a frame, saves it as a JPEG to internal flash
> or a temp location (MicroSD isn't wired up until Phase 7 — use whatever is
> available now, e.g. write to a small internal FS or just log size and discard;
> pick the simplest option and say why), and logs:
> ```
> Camera initialized
> Frame captured
> JPEG created
> ```
>
> Build. This phase needs real hardware to mean anything — if you can't flash and
> test now, say "Build verified; hardware test required" and do not claim the camera
> works.

---

## Phase 6 — Send Photo to Telegram

> Implement Telegram's `sendPhoto` multipart/form-data upload in
> `telegramp4_telegram`, handling HTTPS, correct `Content-Length`, timeouts, retries
> on failure, and full cleanup of any allocated buffers afterward.
>
> Wire up the real `/photo` command (replacing the Phase 4 "Not implemented yet"
> stub and the Phase 3 stub): capture via `telegramp4_camera`, upload via
> `sendPhoto`. Stream the JPEG from wherever Phase 5 stored it rather than holding
> the whole file resident in RAM if avoidable — note in your explanation which
> approach you used and why given ESP32-P4's PSRAM.
>
> Update `/status` free-heap/PSRAM figures to reflect state after a capture+upload
> cycle (should return to baseline — no leaks).
>
> Build and test if hardware + a live bot are available; otherwise mark "Build
> verified; hardware test required."

---

## Phase 7 — MicroSD Storage

> Implement `telegramp4_storage`. First verify the FireBeetle 2 ESP32-P4's MicroSD
> pinout/interface (SDMMC vs SPI mode) from current documentation.
>
> On mount, create if missing:
> ```
> /sdcard/photos/
> /sdcard/videos/
> /sdcard/audio/
> /sdcard/received/
> /sdcard/ai/
> /sdcard/logs/
> ```
>
> Add commands `/files`, `/storage` (used/free/total, spec §7 format), and
> `/delete <filename>`. Implement strict filename sanitization — reject or strip any
> path traversal (`..`, absolute paths, path separators outside the expected
> directory) before touching the filesystem. Add a unit-testable sanitize function
> and mention how you'd test it.
>
> Redirect Phase 5/6 photo capture to save into `/sdcard/photos/` instead of the
> temporary location used before.
>
> Build; hardware test required for actual SD I/O — state clearly whether it was
> performed.

---

## Phase 8 — Photo Gallery

> Implement `/photos`: list recent photos from `/sdcard/photos/` (newest first,
> capped per message — add pagination if the list is long) with inline buttons per
> photo: `[View] [Download] [Delete]`. "View"/"Download" both resolve to `sendPhoto`/
> `sendDocument`; "Delete" must ask for confirmation before removing the file (reuse
> the confirmation pattern from spec §7, matching the `/delete` confirmation used
> elsewhere).
>
> Reuse the Phase 7 filename sanitizer for any filename coming back through a
> callback payload — never trust a callback-supplied filename without validating it
> stays inside `/sdcard/photos/`.
>
> Build and verify pagination logic with a small in-repo test/mock if hardware
> testing isn't available.

---

## Phase 9 — Video

> Implement video recording. First verify what video capture/encoding path is
> actually available for this board/camera on ESP32-P4 (do not assume H.264 —
> confirm against Espressif's current camera/video component support) and pick the
> practical format the hardware actually supports.
>
> Add `/video [seconds]` with configurable min/default/max duration (Kconfig,
> defaults 1/10/60 sec per spec §9). Behavior:
> ```
> 🎥 Recording for 10 seconds...
> ```
> then on completion:
> ```
> Recording complete.
>
> Size: 4.8 MB
> Duration: 10 sec
>
> Uploading...
> ```
> then upload via Telegram (reuse/extend the Phase 6 upload path for larger files;
> check the result stays within Telegram Bot API size limits and reject/warn if not).
> Check free SD space before recording and refuse cleanly if insufficient.
>
> Build. Explicitly state which video format was actually verified to work on
> hardware, or that hardware testing is still required — do not claim H.264 or any
> other format works without having tested it.

---

## Phase 10 — Receive Photo From Telegram

> Handle incoming photos sent to the bot by a user: read `file_id` from `getUpdates`,
> call `getFile`, download to `/sdcard/received/` with a safe generated filename
> (timestamp-based, sanitized), enforcing a configurable max download size (reject
> larger files instead of downloading them).
>
> Reply:
> ```
> 📥 Image received.
>
> File:
> received_20260906_224500.jpg
>
> Size:
> 1.4 MB
> ```
> Add `/photo_info` and `/photo_files` commands to inspect received files.
>
> Build and test the download path against the real Telegram API if possible;
> otherwise state what wasn't verified.

---

## Phase 11 — Audio Recording

> Implement microphone capture (`telegramp4_audio`). Verify the FireBeetle 2
> ESP32-P4's actual microphone interface (I2S/PDM etc.) from current documentation
> before writing the driver code.
>
> Add `/record [seconds]` (Kconfig min/default/max like video). Capture PCM, encode
> to the simplest robust format that's actually practical on this hardware and
> accepted by Telegram (state which format you chose and why — do not assume an
> arbitrary codec is available), save to `/sdcard/audio/`, then upload via Telegram.
> ```
> 🎙 Recording 10 seconds...
> ```
>
> Build; state clearly whether the audio path was tested on real hardware.

---

## Phase 12 — Receive Telegram Voice

> Handle incoming Telegram voice messages the same way Phase 10 handles photos:
> `file_id` → `getFile` → download → save to `/sdcard/received/` with a sanitized,
> timestamped filename. Just save and acknowledge — no decoding or speech
> recognition yet:
> ```
> 🎙 Voice message received.
>
> Duration: 7 sec
> Saved:
> received/voice_001.xxx
> ```
> Enforce the same max-download-size limit as Phase 10.
>
> Build and test against the live API if possible.

---

## Phase 13 — Voice Commands

> Add speech-to-text on top of Phase 12, designed so the STT backend is swappable
> (cloud/external service now, local model later) — do not hardcode one external
> provider into `telegramp4_telegram`; put the STT interface behind its own module
> so the command parser doesn't care which backend produced the text.
>
> Flow: received voice → STT → parsed text → routed through the **same command
> registry** from Phase 3 (a recognized phrase like "take a photo" maps to the
> `/photo` handler; don't create a parallel command execution path).
>
> Example:
> ```
> Speech recognized:
> "take a photo"
>
> Command:
> PHOTO
>
> Executing...
> ```
>
> Be explicit about which STT provider/service you're integrating, what API key or
> config it needs (kept out of Git like every other secret), and any latency/cost
> implications. Build; state what was and wasn't tested live.

---

## Phase 14 — AI Vision

> Implement the `telegramp4_ai` abstraction. Investigate which AI/ML inference
> framework is realistic for ESP32-P4 (e.g. Espressif's ESP-DL) and what pretrained
> object-detection model is small enough to run given this board's PSRAM — report
> your findings, including expected inference time and memory footprint, before
> committing to a model.
>
> ```cpp
> ai_init();
> ai_process_image();
> ai_get_results();
> ai_deinit();
> ```
> Keep the model file/config separate from application code (its own directory,
> documented dependency, license noted per spec §25). Make AI disable-able via
> Kconfig so the rest of the firmware works without it.
>
> Do not claim the model works until you've actually run inference on hardware and
> measured time/memory. If that's not possible now, say so explicitly.

---

## Phase 15 — Telegram AI Workflow

> Add `/ai`: capture via `telegramp4_camera` → preprocess → `telegramp4_ai` inference
> → send both the photo and a formatted result to Telegram:
> ```
> 🤖 AI Detection
>
> Detected:
> 👤 Person — 94%
> 💻 Laptop — 88%
>
> Inference:
> 142 ms
> ```
> Wire the "🤖 AI Detect" button (Phase 4 stub) to this same handler — no duplicated
> logic between button and command.
>
> If AI is disabled via Kconfig (Phase 14), `/ai` should reply cleanly explaining
> it's unavailable rather than crashing.
>
> Build; report actual measured inference time/memory if hardware testing was
> performed, otherwise state it wasn't.

---

## Phase 16 — AI on Telegram Photos

> When a user sends a photo (Phase 10's receive path), instead of only
> acknowledging, offer:
> ```
> Image received.
>
> What would you like to do?
>
> [🤖 Detect Objects]
> [💾 Save]
> [🗑 Delete]
> ```
> "Detect Objects" decodes the received image, preprocesses/resizes it for the
> Phase 14 AI pipeline, runs inference, and returns results (and an annotated image
> if you implement drawing boxes — optional, note if skipped and why). "Save" keeps
> the Phase 10 behavior. "Delete" removes the file after confirmation.
>
> Build and verify the callback routing reuses Phase 14/15 AI code, not a copy.

---

## Phase 17 — Motion Detection

> Implement `telegramp4_motion` for an external PIR sensor on GPIO. Confirm which
> GPIO(s) are safe/available to use on the FireBeetle 2 ESP32-P4 (not already
> claimed by camera/SD/display) before wiring it up — add it as a configurable
> Kconfig pin, not hardcoded.
>
> Add `/arm`, `/disarm`, `/motion` (status). On motion while armed: capture a photo
> → run AI (Phase 14) → if a person is detected, save + prepare a Telegram alert
> (sending the alert itself is Phase 18 — this phase can stop at "person detected,
> logged" if you want a clean boundary, or include the alert if it's natural to do
> together; note which you chose).
> ```
> Motion detection: ARMED
> AI filtering: ON
> ```
> Must not continuously upload images unless explicitly configured — this phase is
> about detection + gating, not spamming Telegram.
>
> Build; hardware (PIR + camera) required to truly verify — state what was tested.

---

## Phase 18 — Motion AI Alert

> Complete the Phase 17 pipeline: motion → photo → AI → if person detected, send a
> Telegram alert to all whitelisted chat IDs:
> ```
> 🚨 Person detected
>
> Confidence: 94%
> Time: 22:45:31
>
> [Photo]
> ```
> Add a configurable cooldown (`motion_alert_cooldown`, default 30s) so repeated
> triggers don't spam Telegram — track last-alert timestamp per armed session.
>
> Build and verify the cooldown logic (this can be tested without hardware — write
> it so the timing logic is testable independent of the PIR interrupt). State
> whether the full hardware path (PIR → camera → AI → Telegram) was verified live.

---

## Phase 19 — GPIO / IoT Control

> Implement `telegramp4_gpio` with a **whitelist** of configurable GPIOs (Kconfig) —
> never expose arbitrary/dangerous pins automatically. Add `/gpio` (list configured
> pins + state), `/gpio <pin> on`, `/gpio <pin> off`, and an inline-button version:
> ```
> 🔌 GPIO Control
>
> GPIO 1 [ON]
> GPIO 2 [OFF]
> GPIO 3 [ON]
> ```
> Document (in the lesson) how to wire an LED/relay/buzzer/button to a whitelisted
> pin. Reject any request for a non-whitelisted pin with a clear error, not a crash.
>
> Build; hardware test optional/depends on what peripherals are attached — state
> what was verified.

---

## Phase 20 — Display

> Implement `telegramp4_display` for an optional MIPI-DSI display. Verify actual
> display support/connector for this board — this is explicitly optional hardware,
> so the component must compile out cleanly (Kconfig) and the rest of the firmware
> must work identically with no display attached.
>
> Show system status:
> ```
> TelegramP4

> WiFi ✓
> Telegram ✓
> Camera ✓
> SD ✓
> AI ✓

> IP:
> 192.168.1.50
> ```
> and optionally a camera preview view. Keep this phase scoped to status +
> preview — don't add new Telegram commands here.
>
> Build with display enabled and disabled (two build configs) if possible; state
> which was actually tested on hardware.

---

## Phase 21 — OTA

> Implement OTA update support using `esp_https_ota`, appropriate partition table
> updates (`partitions/partitions.csv` already has OTA slots from Phase 0 — verify
> and adjust if needed), version tracking, and rollback where the ESP-IDF OTA APIs
> support it. Manual/local OTA trigger is acceptable for this phase; remote/
> automatic OTA can come later.
>
> Add `/version`:
> ```
> TelegramP4

> Firmware:
> 1.0.0

> ESP-IDF:
> x.x.x

> Board:
> FireBeetle 2 ESP32-P4
> ```
>
> Report free flash space and confirm sufficient space for OTA slots. State whether
> an actual OTA update cycle was tested on hardware.

---

## Phase 22 — System Status (final integration)

> Flesh out `/status` into the full dashboard (spec §22 exact format): firmware
> version, Wi-Fi (connected/IP/RSSI), Telegram connectivity, camera readiness, SD
> card readiness + free space, AI readiness + last inference time, heap/PSRAM,
> uptime. Never include credentials.
>
> Add `/diagnostics` (spec §21): CPU, heap, PSRAM, storage, camera FPS, JPEG size, AI
> inference time, Wi-Fi RSSI, Telegram round-trip latency.
>
> Assemble the final menu (spec §19) with all buttons wired to their real handlers,
> add confirmation prompts for `/reboot` and `/delete` if not already present
> (`Are you sure? [Yes] [Cancel]`), and do a full pass through the testing checklist
> in spec §20. Update `README.md`'s feature list and `CHANGELOG.md` for the 1.0.0
> release. Update every lesson doc under `docs/lessons/` to match final behavior.
>
> This is the integration milestone from spec §27 — walk through the full
> success-criteria list and report, item by item, what's confirmed working on real
> hardware vs. still unverified.

---

## After Phase 22

Remaining spec sections (§26 Future Extensions: MQTT, Home Assistant, web dashboard,
face recognition, etc.) are intentionally out of scope until the core platform is
solid and requested. Re-derive new phase prompts the same way if the project grows.
