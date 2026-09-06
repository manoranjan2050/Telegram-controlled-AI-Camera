# Lesson 15 — OTA Updates (Phase 21)

## Goal

Add `/version` and `/ota <url>` for manual, URL-based firmware updates.

## What you learn

- Unlike most recent phases, this one is **not** blocked on hardware
  verification — `esp_https_ota` and the two-OTA-slot partition table
  (reserved back in Phase 0) are standard ESP-IDF functionality that works
  the same on any ESP32 target
- Using the ESP-IDF certificate bundle for OTA downloads too, not just
  Telegram API calls — TLS is never disabled anywhere in this project
- Gating a powerful, one-shot action (flashing arbitrary firmware from a URL)
  behind an explicit Kconfig opt-in (`TELEGRAMP4_OTA_ENABLED`, default off)

## Code

- [`components/telegramp4_ota/`](../../components/telegramp4_ota/) —
  `telegramp4_ota_update_from_url()`
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `OTA`
  submenu: `TELEGRAMP4_OTA_ENABLED`
- [`main/app_main.cpp`](../../main/app_main.cpp) — `/version`, `/ota <url>`

## How it works

`telegramp4_ota_update_from_url()` wraps ESP-IDF's single-call
`esp_https_ota()` API: it downloads the image at `url` over HTTPS (cert
bundle validated), verifies it, writes it to the inactive OTA partition, and
marks it bootable. On success it calls `esp_restart()` directly — the
function does not return in that case. `/ota` in `app_main.cpp` sends an
"update starting" message first, since there won't be a chance to reply after
a successful reboot.

`/version` reports firmware version (`TELEGRAMP4_FIRMWARE_VERSION` from
`telegramp4_board.h`), the ESP-IDF version (`esp_get_idf_version()`), and the
board name — all local, no hardware verification needed.

**Not yet implemented:** a `[Yes]/[Cancel]` confirmation before `/ota` runs.
Per the master spec, that lands project-wide (alongside `/reboot`, `/delete`)
in Phase 22.

## Test

```bash
idf.py menuconfig   # TelegramP4 Configuration -> OTA -> enable to test
idf.py build
idf.py -p <PORT> flash monitor
```
`/version` should work immediately. `/ota <url>` requires hosting a valid
signed `.bin` somewhere reachable over HTTPS — test carefully, since a bad
image (or accidentally OTA-ing to the same partition on a single-slot setup)
can require re-flashing over USB to recover. Not tested against a real OTA
server in this session — "Build verified; hardware test required."

## Next lesson

[Lesson 16 — Final integrated project](16-final-project.md)
