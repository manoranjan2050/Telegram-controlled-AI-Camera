# Lesson 14 — Optional Display (Phase 20)

## Goal

Add an optional MIPI-DSI status display that doesn't change behavior at all
when absent.

## ⚠️ Hardware verification status

The display connector/panel/driver for this board is unverified.
`telegramp4_display_init()` is an honest stub. The component is
compile-time optional (`TELEGRAMP4_DISPLAY_ENABLED`, default off).

## What you learn

- Keeping an entire optional peripheral's cost at zero when disabled: the
  status task is only spawned if `telegramp4_display_init()` succeeds, and
  `telegramp4_display_update_status()` itself no-ops when the feature is off
- Aggregating status from multiple already-existing modules (WiFi, camera,
  storage, AI) into one small snapshot struct for a periodic refresh, without
  adding new dependencies between those modules themselves

## Code

- [`components/telegramp4_display/`](../../components/telegramp4_display/) —
  `telegramp4_display_init/is_enabled/update_status` (stub)
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `Display`
  submenu: `TELEGRAMP4_DISPLAY_ENABLED`
- [`main/app_main.cpp`](../../main/app_main.cpp) — `display_status_task()`,
  spawned only if the display actually initialized

## How it works

`display_status_task()` runs every 5 seconds, reading WiFi/camera/storage/AI
state from their respective components (`app_main.cpp` is the one place that
already knows about all of them) and calling
`telegramp4_display_update_status()`. Since the display component isn't
wired into any other module, and the task is only created when
`telegramp4_display_init()` actually returns `ESP_OK`, a board without a
display attached (or with it disabled in Kconfig) pays zero runtime cost for
this feature — matching the master spec's requirement that "the device must
still work without the display."

## Test

```bash
idf.py build   # display disabled by default - firmware behaves exactly as before
```
With `TELEGRAMP4_DISPLAY_ENABLED=y` and no real panel driver yet, expect a
logged `Display hardware not yet verified for this board` and no crash — the
rest of the firmware runs normally.

## Next lesson

[Lesson 15 — OTA updates](../PHASES.md#phase-21--ota)
