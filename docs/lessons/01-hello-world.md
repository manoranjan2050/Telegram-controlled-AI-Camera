# Lesson 01 — Hello ESP32-P4

## Goal

Get a clean, minimal ESP-IDF project building and running on the DFRobot FireBeetle
2 ESP32-P4 AI Vision Board, with a proper component structure to build on.

## What you learn

- How an ESP-IDF project is laid out (`CMakeLists.txt`, `main/`, `components/`)
- How to create a reusable component (`telegramp4_board`) instead of putting
  everything in `app_main`
- How to bring up NVS (needed by almost every later phase) and ESP-IDF logging
- How the project's partition table and Kconfig scaffolding are structured for
  features that don't exist yet (OTA slots, a configuration menu)

## Hardware

DFRobot FireBeetle 2 ESP32-P4 AI Vision Board, connected via USB for flashing and
serial monitoring. No camera, SD card, or peripherals required for this lesson.

## Wiring

None — onboard USB only.

## Software

- ESP-IDF v5.4.x with `esp32p4` target support
- A serial terminal (built into `idf.py monitor`)

## Code

- [`main/app_main.cpp`](../../main/app_main.cpp) — entry point: init NVS, print banner
- [`components/telegramp4_board/`](../../components/telegramp4_board/) — board
  identity component (`telegramp4_board_print_banner()`)
- [`partitions/partitions.csv`](../../partitions/partitions.csv) — two-OTA-slot
  partition table, reserved now so Phase 21 doesn't need to repartition
- [`Kconfig.projbuild`](../../Kconfig.projbuild) — empty `TelegramP4 Configuration`
  menu shell that later phases populate

## How it works

`app_main()` initializes NVS flash (erasing and retrying once if the NVS partition
is out of free pages or has a newer-than-supported version — the standard ESP-IDF
pattern), then calls `telegramp4_board_print_banner()`, which logs the board name
and firmware version through the `TAG_BOARD` log tag. This keeps board identity
information in one component instead of scattered string literals in `app_main`.

## Test

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

## Expected output

```
I (xxx) TAG_BOARD: TelegramP4 starting...
I (xxx) TAG_BOARD: Board: FireBeetle 2 ESP32-P4
I (xxx) TAG_BOARD: Firmware version: 0.1.0
I (xxx) TAG_SYSTEM: Phase 0 bootstrap complete.
```

## Common errors

- **"unsupported target"** — run `idf.py set-target esp32p4` before building.
- **Flash fails / port busy** — close any other serial monitor and confirm the
  correct `<PORT>`.
- See [docs/troubleshooting.md](../troubleshooting.md) for more.

## Challenge

Add a second log line printing the ESP-IDF version (`esp_get_idf_version()`) next
to the firmware version — useful groundwork for the `/version` command in Phase 21.

## Next lesson

[Lesson 02 — Wi-Fi connection](02-wifi.md)
