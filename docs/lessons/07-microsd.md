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

## Next lesson

[Lesson 08 — MicroSD storage / photo gallery](09-receive-photo.md) — Phase 8
(gallery UI) is covered together with Phase 9/10 lessons as those land, since
the original lesson file list groups them.
