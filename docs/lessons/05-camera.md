# Lesson 05 — Camera Capture

## Goal

Real capture: `telegramp4_camera` drives an OV5647 sensor via MIPI-CSI using
Espressif's `esp_video` + `esp_cam_sensor` (V4L2-style API), producing an
actual JPEG frame for `/photo_test`/`/photo`.

## What was unverified, and how it got resolved

The initial version of this component (2026-09-06) was an honest stub — the
camera sensor and driver stack weren't known, and web access was unavailable
in that session. On 2026-09-07, two things resolved this:
1. The user physically installed a camera module and read its silkscreen: a
   **Raspberry Pi Camera Module v1.3**, which uses the **OV5647** sensor —
   officially supported by Espressif's `esp_cam_sensor` over MIPI-CSI on
   ESP32-P4.
2. The user provided DFRobot's actual DFR1172 schematic PDF. Reading it
   directly (rendered to images, inspected pin-by-pin) confirmed: SCCB
   (I2C) SCL=GPIO8/SDA=GPIO7, and — importantly — **no reset, power-down, or
   XCLK signal is routed from the P4 to the camera connector at all**. The
   sensor is MIPI-clocked with no separate host-driven clock pin, which is
   why `TELEGRAMP4_CAMERA_RESET_PIN`/`PWDN_PIN`/`XCLK_PIN` all default to -1.

See [docs/hardware.md](../hardware.md) for the full confirmed pin table and
schematic links.

## What you learn

- Espressif's V4L2-style camera API on ESP32-P4: `esp_video_init()` brings up
  the sensor over SCCB (I2C), then a capture device (`ESP_VIDEO_MIPI_CSI_DEVICE_NAME`)
  and a JPEG M2M (memory-to-memory) encoder device
  (`ESP_VIDEO_JPEG_DEVICE_NAME`) are driven with standard `open`/`ioctl`/`mmap`
  calls (`VIDIOC_S_FMT`, `VIDIOC_REQBUFS`, `VIDIOC_QBUF`/`DQBUF`,
  `VIDIOC_STREAMON`)
- Zero-copy handoff: the raw capture buffer is queued straight into the JPEG
  encoder's output side via `V4L2_MEMORY_USERPTR`, instead of copying pixels
  around
- Keeping the capture+encode pipeline streaming continuously (started once in
  `telegramp4_camera_init()`) rather than restarting it per photo, for lower
  per-request latency — `telegramp4_camera_capture()` just does one
  DQBUF→QBUF→DQBUF→QBUF cycle and copies the resulting JPEG bytes to a heap
  buffer before returning any V4L2 buffer to its queue

## ✅ Resolved 2026-09-09: PSRAM boot-loop fixed, real capture confirmed live

The PSRAM/boot-loop issue described below (kept for history) is fixed — see
`main/freertos_static_mem.c` and the "Current status" section of
[CLAUDE.md](../../CLAUDE.md) for the full root cause (TCM-backed FreeRTOS
static task memory) and fix (linker `--wrap` overrides + a larger
`main_task` stack, both project-local, no ESP-IDF/SDK files touched).

With PSRAM stable, `/dev/video10` (the JPEG M2M encoder device) still failed
to open at first. Root cause: `CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_ENC_VIDEO_DEVICE`
defaults to `n` in the `espressif/esp_video` managed component's own
Kconfig — the JPEG device simply isn't registered without it, regardless of
memory state. Enabling it in `sdkconfig.defaults` was the last piece.

**Confirmed live**: `Camera initialized (800x800)`, and `/photo` produces
real JPEG frames — 15155 and 38432 bytes observed in the same test session,
both delivered to Telegram.

<details>
<summary>Original 2026-09-07 note (kept for history)</summary>

`telegramp4_camera_capture()` needs PSRAM for its frame buffers (two 800x800
RAW8 capture buffers alone are ~1.28MB, well over the ~768KB internal L2MEM
budget). Enabling `CONFIG_SPIRAM=y` makes PSRAM itself initialize correctly
("Found 32MB PSRAM device", memory test OK), but something about it breaks
ESP-Hosted's very early static task creation
(`xPortCheckValidTCBMem` assertion, boot loop) **before `app_main()` even
runs** — unrelated to the camera code itself, and not yet root-caused. PSRAM
is disabled again in `sdkconfig.defaults` for now so WiFi/Telegram/SD stay on
the last known-stable, tested configuration. Until this is resolved,
`/photo_test` will fail at the buffer-allocation step even though the sensor
bring-up code itself is real, not a stub.

</details>

## ✅ Resolved 2026-09-11: fixed dark/underexposed photos and video

Once real captures were flowing, the next real-world problem showed up:
photos and video both came out noticeably dark — recognizable but dim, and
genuinely too dark in low light. The user asked directly whether this was
a camera hardware issue or a code issue; it was code, and root-causing it
took two attempts.

**First attempt (wrong mechanism)**: the OV5647 driver
(`esp_cam_sensor`'s `ov5647.c`) exposes an `ESP_CAM_SENSOR_EXPOSURE_VAL`
parameter (an AE *target* register, default `0x50`/80 out of 2-235) mapped
to the standard `V4L2_CID_EXPOSURE` control. Setting it via
`ioctl(fd, VIDIOC_S_CTRL, ...)` seemed like the obvious fix - it compiled,
ran, and silently failed every time (`Failed to raise AE target`). Reading
`esp_video_ioctl.c` explained why: this esp_video version only implements
the **extended** controls API (`VIDIOC_S_EXT_CTRLS`/`struct
v4l2_ext_controls`) - the simple, singular `VIDIOC_S_CTRL` isn't wired up
for camera controls at all, so every call to it was a no-op wrapped in a
misleadingly-generic ioctl error.

**Real root cause**: reading `esp_video_init.c` (not guessing) revealed
`CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER` (default `n`). This gates
Espressif's *entire* closed-loop 3A controller - a dedicated `isp_task`
that continuously reads ISP hardware statistics, runs the IPA
auto-exposure/auto-gain/auto-white-balance algorithms
(`managed_components/espressif__esp_ipa/`, already vendored but never
actually running), and feeds the results back to the sensor via
`VIDIOC_S_EXT_CTRLS`. With it off, the camera had been running this whole
time on the sensor's raw power-on register defaults with **no active
exposure control whatsoever** - not a slightly-wrong setting, a completely
disabled feedback loop. Enabling the one Kconfig flag starts the
controller automatically as part of the existing `esp_video_init()` call;
no application code change was needed for the fix itself.

Also bumped the pre-capture "let auto-exposure settle" frame skip from 2
to 12 in both the photo pipeline (`start_pipeline()`) and video mode
(`telegramp4_camera_start_video_mode()`, which previously had no settle
step at all) - a real closed-loop controller needs several feedback
cycles to converge, not one or two frames.

**Confirmed visually**: captured a photo before and after via a temporary
base64-over-serial dump (real end-to-end verification, not just checking
that a JPEG file was produced) - the "before" frame was almost entirely
black with a single bright accent point; the "after" frame, taken in the
same dim room, clearly showed "RADEON" text on a graphics card, individual
fan blades, cables, and RGB lighting detail. Still a dark room in absolute
terms, but the difference between "unusable" and "a real photo" was stark
and immediate.

## Code

- [`sdkconfig.defaults`](../../sdkconfig.defaults) —
  `CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER=y`

- [`components/telegramp4_camera/`](../../components/telegramp4_camera/) —
  `telegramp4_camera_init/deinit/capture/release_frame/get_status`, real
  V4L2-based implementation
- [`components/telegramp4_camera/idf_component.yml`](../../components/telegramp4_camera/idf_component.yml) —
  `espressif/esp_video`, `espressif/esp_cam_sensor` (esp32p4 only)
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — `Camera` submenu
  with the confirmed SCCB pins as defaults
- [`sdkconfig.defaults`](../../sdkconfig.defaults) — `CONFIG_CAMERA_OV5647=y`

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/photo_test` or `/photo` in Telegram. Expect a real JPEG size logged
(confirmed live) and, for `/photo`, an actual photo delivered to the chat.

## Next lesson

[Lesson 06 — Send photo to Telegram](06-send-photo.md)
