# Lesson 05 — Camera Capture

## Goal

Establish the camera abstraction (`telegramp4_camera`) and a `/photo_test`
command, ready for the real sensor driver to be wired in once verified against
hardware.

## ⚠️ Hardware verification status

The exact camera sensor for the FireBeetle 2 ESP32-P4 AI Vision Board, and which
Espressif driver stack applies (`esp32-camera` vs. the newer `esp-video`/
`esp_cam_sensor` stack — ESP32-P4 uses MIPI-CSI, not the classic DVP interface),
were **not verified** against live documentation while writing this code (web
access was unavailable in that session). `telegramp4_camera_init()` and
`telegramp4_camera_capture()` are therefore honest stubs: they compile, integrate
correctly with the rest of the firmware, and return
`ESP_ERR_NOT_SUPPORTED`/log a clear error rather than pretending to work.

**Before this lesson is actually complete**, someone with the board must:
1. Confirm the exact camera sensor and connector against DFRobot's product page/
   wiki.
2. Pick the correct driver component (`esp32-camera` or `esp-video` +
   `esp_cam_sensor`) and add it to `main/idf_component.yml`.
3. Fill in `telegramp4_camera_init()`/`capture()` in
   [`telegramp4_camera.c`](../../components/telegramp4_camera/telegramp4_camera.c)
   with the real init sequence and pin mapping.
4. Update [docs/hardware.md](../hardware.md) with the confirmed values.
5. Actually test capture on the board before claiming this phase works.

## What you learn (once hardware is wired in)

- The `camera_init/deinit/capture/release_frame/get_status` abstraction pattern
- Why buffer ownership (`telegramp4_camera_frame_t` + explicit release) matters
  for memory safety with JPEG frames
- Kconfig-driven capture parameters instead of hardcoded resolution/quality

## Code

- [`components/telegramp4_camera/`](../../components/telegramp4_camera/) — the
  abstraction: `telegramp4_camera_init/deinit/capture/release_frame/get_status`
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `Camera` submenu:
  `TELEGRAMP4_CAMERA_JPEG_QUALITY`, `TELEGRAMP4_CAMERA_FRAME_WIDTH/HEIGHT`
- [`main/app_main.cpp`](../../main/app_main.cpp) — `/photo_test` command; calls
  `telegramp4_camera_init()` at boot (logs a warning and continues without camera
  if it fails, rather than halting the whole device)

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
In Telegram: `/photo_test`. Until the driver is wired in, expect:
```
❌ Camera unavailable.
Check camera connection.
```
This is the correct, honest behavior for this phase — not a bug.

## Next lesson

[Lesson 06 — Send photo to Telegram](06-send-photo.md) (also blocked on the same
hardware verification step above).
