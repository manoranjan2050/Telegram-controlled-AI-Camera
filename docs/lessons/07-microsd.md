# Lesson 07 — MicroSD Storage

## Goal

Mount the MicroSD card, create the standard directory layout, and add
`/files`, `/storage`, `/delete` — with every filename sanitized before it
touches the filesystem.

## ⚠️ Hardware verification status

SDMMC pins default to **ESP-IDF's own SoC-level reference default for
ESP32-P4** (`SDMMC_SLOT_CONFIG_DEFAULT()`: CLK=43, CMD=44, D0=39, D1=40, D2=41,
D3=42 — found directly in the installed ESP-IDF v5.4 source at
`components/esp_driver_sdmmc/include/driver/sdmmc_default_configs.h`). This is
a real, documented default for the chip, not a guess — but it is **not**
confirmed to match how DFRobot wired the SD slot on this specific board. If
mounting fails on real hardware, this is the first thing to check against
DFRobot's schematic (see [docs/hardware.md](../hardware.md)).

## What you learn

- Mounting a FAT-formatted SD card with `esp_vfs_fat_sdmmc_mount()`
- Why file safety code belongs in one place (`telegramp4_storage_sanitize_path`)
  instead of being reimplemented per command
- An allow-list approach to sanitization: strip everything that isn't
  alphanumeric/`-`/`_`/`.` rather than trying to blacklist dangerous characters

## Code

- [`components/telegramp4_storage/`](../../components/telegramp4_storage/):
  - `telegramp4_storage_init()` — mounts `/sdcard`, creates
    `photos/videos/audio/received/ai/logs`
  - `telegramp4_storage_get_status()` — used/free/total bytes
  - `telegramp4_storage_sanitize_path()` — the one function every later phase
    that touches a Telegram-supplied filename must call
- [`main/app_main.cpp`](../../main/app_main.cpp) — `/files`, `/storage`,
  `/delete <filename>`; `/photo` now also saves a copy to `/sdcard/photos/`

## How it works

`telegramp4_storage_sanitize_path(subdir, raw_name, out, out_len)` first checks
`subdir` against a fixed list of directories this component itself created —
never a caller-supplied string — then builds a filtered copy of `raw_name`
keeping only `[a-zA-Z0-9._-]` characters (this also naturally defeats `../`
traversal, since `/` and `.` sequences beyond a single dot are stripped/rejected
outright) and refuses names that would end up empty or starting with `.`. Only
if all of that passes does it write the final absolute path.

`/delete <filename>` runs the filename through this function before calling
`remove()` — an unsafe filename is rejected with `❌ Invalid filename.` before
ever reaching the filesystem. A full `[Yes] [Cancel]` confirmation UI for
destructive actions (`/delete`, `/reboot`) is planned project-wide for Phase 22;
for now `/delete` acts immediately once the filename passes sanitization.

`/files` reports a per-directory file count (the interactive gallery with
View/Download/Delete buttons is Phase 8). `/storage` reports used/free/total
space, formatted in GB/MB.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
Insert a FAT32-formatted MicroSD card. In Telegram: `/storage`, `/files`. If
the card mounts, expect real numbers; if SDMMC pins don't match this board,
expect a logged mount failure and `❌ Storage full.\nSD card not mounted.` from
the commands — check the pin mapping against docs/hardware.md next.

## Common errors

- Mount fails — likely a pin mismatch (see the warning above) or a card not
  formatted FAT32. See [docs/troubleshooting.md](../troubleshooting.md).

## Phase 8 update — Telegram photo gallery

`/photos` lists up to 8 recent photos from `/sdcard/photos/` (newest first),
each with a row of `[View] [Download] [🗑 Delete]` inline buttons. This needed
one addition to `telegramp4_telegram`: a generic
`telegramp4_telegram_send_with_keyboard(chat_id, text, reply_markup_json)` (the
Phase 4 main menu now calls this too, instead of having its own copy of the
send-with-keyboard logic), plus `telegramp4_telegram_send_document()` for
"Download" (Telegram's `sendDocument`, sharing the same multipart-upload
internals as `send_photo`).

Each button's `callback_data` encodes an action and a filename —
`"/photo_view <name>"`, `"/photo_dl <name>"`, `"/photo_del <name>"` — dispatched
through the exact same command registry as everything else (they're registered
commands, not a special case). Deleting from the gallery and deleting via
`/delete <filename>` both call one shared `delete_photo_file()` function, so
there's a single place that sanitizes and removes a file no matter which UI
path triggered it — and every filename arriving through a callback is
sanitized just as strictly as typed input, since a callback payload is
just as untrusted.

Test: `/photo` a few times to populate the gallery, then `/photos`, then try
each button.

## Next lesson

[Lesson 08 — Video recording](08-video.md)
