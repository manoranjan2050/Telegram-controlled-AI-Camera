# Lesson 07 — MicroSD Storage

## Goal

Mount the MicroSD card, create the standard directory layout, and add
`/files`, `/storage`, `/delete` — with every filename sanitized before it
touches the filesystem.

## ✅ Hardware confirmed 2026-09-07

SDMMC pins use **ESP-IDF's own SoC-level reference default for ESP32-P4**
(`SDMMC_SLOT_CONFIG_DEFAULT()`: CLK=43, CMD=44, D0=39, D1=40, D2=41, D3=42).
Reading DFRobot's actual DFR1172 schematic confirmed these are exactly the
pins this board wires the MicroSD socket to (`SD1_*` nets, schematic page 1) —
not a coincidence, this board follows the chip's reference pinout. Mounting on
`SDMMC_HOST_SLOT_0` (not the default `SLOT_1`, which the WiFi/C6 link uses)
was the first real hardware fix.

The schematic also revealed something the pins alone don't tell you: the SD
socket's power (VDD) is switched by a P-MOSFET (`Q1`, AO3401) gated by
**GPIO45** (net `SD1_PWRN`). `telegramp4_storage_init()` configures GPIO45 as
an output and waits 100ms for the supply rail to stabilize before touching
the SDMMC bus.

**Real-hardware result: this alone did not fix the mount.** Both polarities
(GPIO45 driven LOW and HIGH) were tested live and produced byte-for-byte
identical `0x107`/`ESP_ERR_TIMEOUT` failures at the same point every time —
which means this pin is very likely *not* the actual blocking factor (a
genuine polarity bug would behave differently one way vs. the other).

**2026-09-10 follow-up**: also tried enabling `SDMMC_SLOT_FLAG_INTERNAL_PULLUP`
on the CMD/D0-D3 lines (confirmed enabled in the boot log —
`gpio: GPIO[43]|...|Pullup: 1` etc.) in case the bus was floating without
pull-ups. **Identical failure, same point, same error code.**

**Then traced the actual SD sheet of the schematic (page 4 of 8) directly**,
not just the pin table, to check *why* neither fix moved the needle - and
found the answer in the circuit itself: `R35`-`R39` are real 51K pull-up
resistors already wired from the 3.3V rail to CMD/D0/D1/D2/D3 in hardware,
and the power-gate MOSFET (`Q1`) has its gate pulled down by `R4` by
default, meaning the card is powered **even before firmware touches
GPIO45**. Both leading firmware theories were never actually viable - the
board was always correctly powered and correctly pulled-up, which is
exactly why changing either in software produced zero difference. This
isn't a guess anymore; it's read directly off the circuit.

Four independent firmware variables tried (pins, slot, power-gate polarity
x2, pull-ups), two of them proven redundant by the schematic itself, all
produced the exact same `send_op_cond` timeout at the exact same point —
strong enough evidence at the time to suspect a physical problem (bad
card, bad socket) that firmware couldn't fix. The user tried a second,
freshly-FAT32-formatted, PC-verified-healthy card. **Identical failure.**

## ✅ SOLVED 2026-09-10 — it was never hardware. It was init order.

The user asked directly whether an official factory/vendor test existed to
check independently. DFRobot publishes exactly that: an official
MicroPython example for this board
([wiki.dfrobot.com/dfr1172/docs/22904](https://wiki.dfrobot.com/dfr1172/docs/22904))
using the identical pins/slot (`SDCard(slot=0, width=4, sck=43, cmd=44,
data=(39,40,41,42), freq=40000000)`). Flashing MicroPython (v1.29.0,
`ESP32_GENERIC_P4-PRE_REV3_C6_WIFI` build, matching this board's chip
revision v1.0 and C6 coprocessor) and running that exact script mounted
the card cleanly on the first try:

```
>>> print(os.listdir('/sd'))
['System Volume Information']
```

With WiFi never touched, the card that had "failed" through four rounds of
firmware changes and two different physical cards worked instantly. That
one data point reframed everything: the SD card (slot 0) and the ESP32-C6
WiFi link (SDIO on slot 1) share **the same physical SDMMC/SDIO host
controller**. This project's `app_main()` always called
`telegramp4_wifi_init()` before `telegramp4_storage_init()` — WiFi's SDIO
bring-up was leaving shared host-peripheral state that slot 0's own init
could never recover from, producing an identical, unmoving failure
regardless of pins, power, or pull-ups, because none of those were ever
the actual problem.

**Fix**: reorder `app_main()` to mount SD *before* starting WiFi (and
init the camera before either, since it needs its own DMA channel - see
the comment in `main/app_main.cpp`). Confirmed live: camera, SD, and WiFi
all now succeed in the same boot.

Getting all three to succeed together for the first time surfaced a
second, separate bug: `CONFIG_VFS_MAX_COUNT` (default 8) is a fixed-size
table of *registered VFS backends* (not open files) - stdio/eventfd, the
camera's two `/dev/videoN` device nodes, and the SD FATFS mount filled it
up before lwip could register its own socket range when WiFi started,
aborting with `ESP_ERR_NO_MEM`. Raised to 16 in `sdkconfig.defaults`.

**Lesson for next time**: when a peripheral fails identically across every
plausible fix, check what else shares its underlying hardware block and
what order things initialize in - not just the peripheral's own pins and
power. And when in doubt, an official vendor test using different code
entirely is worth more than another round of guessing.

<details>
<summary>Original 2026-09-06 note (kept for history)</summary>

This is
a real, documented default for the chip, not a guess — but it is **not**
confirmed to match how DFRobot wired the SD slot on this specific board. If
mounting fails on real hardware, this is the first thing to check against
DFRobot's schematic (see [docs/hardware.md](../hardware.md)).

</details>

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
