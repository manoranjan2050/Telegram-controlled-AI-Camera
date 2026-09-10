# Lesson 09 — Video Recording (file: `08-video.md`, Phase 9)

> Note on numbering: this file keeps the name `08-video.md` from the original
> repository layout (spec §5), even though it corresponds to build Phase 9 —
> the lesson-file list and the phase list in the master spec don't share the
> same numbering, and file names were kept stable rather than renumbered.

## Goal

Add `/video [seconds]`, recording real H.264 video using the ESP32-P4's
hardware encoder and delivering it to Telegram, without blocking command
handling for other chats while it runs.

## ✅ Confirmed 2026-09-10: real hardware H.264 recording works

`espressif/esp_video` exposes the P4's hardware H.264 encoder the exact
same way it exposes the JPEG encoder used for photos (Lesson 05): a V4L2
M2M device, `/dev/video11` (`ESP_VIDEO_H264_DEVICE_NAME`), registered only
when `CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE=y` (defaults to `n` in
the managed component's own Kconfig — the same trap that blocked JPEG
capture until it was found).

### The pixel-format problem

The H.264 encoder needs its input already in a packed YUV 4:2:0 layout
(`V4L2_PIX_FMT_YUV420` in V4L2 terms, `ESP_H264_RAW_FMT_O_UYY_E_VYY`
internally — confirmed by reading `esp_video_h264_device.c` directly: it
passes the input buffer straight to the hardware encoder with **no
conversion of its own**). That's a different format than the
RGB565/UYVY/RGB24/GREY the photo pipeline picks for the JPEG path.

Rather than write a software pixel-format converter (slow, and this chip
has no spare CPU headroom to burn on it), the fix was to have the ISP
itself produce the right format: `esp_video_csi_format.c`'s own format
table lists `V4L2_PIX_FMT_YUV420` as a real ISP output color option
(`ISP_COLOR_YUV420`). Setting `VIDIOC_S_FMT` on the *capture* device to
`V4L2_PIX_FMT_YUV420` makes the hardware ISP hand back frames already in
exactly the layout the encoder needs — zero-copy, zero software
conversion, confirmed by reading the driver's source, not guessed.

### Photo and video can't run at the same time

The MIPI-CSI capture engine only runs one pixel format at a time, so photo
mode (RGB565/UYVY for JPEG) and video mode (YUV420 for H.264) are mutually
exclusive. `telegramp4_camera_start_video_mode()` tears the photo pipeline
down first; `telegramp4_camera_stop_video_mode()` tears the video pipeline
down and calls `telegramp4_camera_init()` again automatically, so `/photo`
works again the moment a `/video` recording finishes — confirmed live:
the exact same boot log that showed a successful 5-second recording also
showed the photo pipeline re-initializing immediately afterward with no
manual intervention.

**Confirmed live** via a temporary diagnostic hook (call
`telegramp4_video_record()` directly at boot, bypassing Telegram, to get a
fast answer without waiting through a long accumulated Telegram command
backlog from earlier testing):

```
I (4777) TAG_CAMERA: Video mode started (800x800, H.264 @ 1000000 bps)
I (10077) TAG_VIDEO: Recorded 37 frames, 36816 bytes of H.264, 5s
E (10077) DIAG: video_record returned ESP_OK, len=36816, duration=5
I (10077) TAG_CAMERA: Camera initialized (800x800)   <- photo mode auto-restored
```

### Output format: raw H.264, not MP4

The encoder produces an Annex-B elementary stream (start-coded NAL units,
SPS/PPS auto-prepended on the first frame) — there's no MP4 container.
`/video` delivers it to Telegram as a document named `video.h264`, which
VLC/ffplay open directly. Proper MP4 muxing (so Telegram previews it
inline as a video) is a reasonable follow-up, not required for this to be
a real, working feature.

### Known minor issue

Setting the encoder's bitrate via `VIDIOC_S_CTRL(V4L2_CID_MPEG_VIDEO_BITRATE, ...)`
currently fails (`W TAG_CAMERA: Failed to set H.264 bitrate control, using
device default`) — non-fatal, the encoder falls back to its own default
and still produces valid video, but the requested 1 Mbps target isn't
actually being applied yet. Worth revisiting if output file size becomes a
problem for longer recordings.

## What you learn

- ESP-IDF's V4L2 M2M pattern is genuinely reusable across codecs (JPEG,
  H.264) — the open/S_FMT/REQBUFS/QBUF/STREAMON dance is identical, only
  the device node and pixel formats change
- Reading a driver's actual source beats trusting a format name: V4L2's
  "`YUV420`" label here does *not* mean standard planar YUV420 — it's an
  ESP-specific packed layout, and the only way to know that for certain
  was reading `esp_video_h264_device.c`'s own input-format mapping
- Mutually-exclusive hardware pipelines (one capture engine, two possible
  output formats) are a real constraint worth designing around explicitly
  (start/stop video mode) rather than trying to keep both running
- Why long-running operations (recording) must run on their own FreeRTOS
  task, never on the Telegram long-poll task — otherwise every other
  chat's commands stall for the duration of the recording

## Code

- [`components/telegramp4_camera/telegramp4_camera.c`](../../components/telegramp4_camera/telegramp4_camera.c) —
  `telegramp4_camera_start_video_mode/read_video_frame/stop_video_mode`
- [`components/telegramp4_video/`](../../components/telegramp4_video/) —
  `telegramp4_video_record()`: opens video mode, pulls encoded frames for
  `duration_s` seconds into a heap (PSRAM) buffer, closes video mode
- [`sdkconfig.defaults`](../../sdkconfig.defaults) —
  `CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE=y`,
  `CONFIG_VFS_MAX_COUNT=20` (a third `/dev/videoN` node needed more headroom
  in the shared VFS table — see docs/lessons/07-microsd.md for the first
  time this table filled up)
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — `Video` submenu:
  min/default/max duration (1/10/60s)
- [`main/app_main.cpp`](../../main/app_main.cpp) — `handler_video()` /
  `video_record_task()`: same pattern as audio/photo — best-effort SD save,
  then upload regardless of SD state

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/video 5` in Telegram. Expect "🎥 Recording for 5 seconds...", then
"Uploading...", then a `video.h264` file — confirmed to actually contain
real recorded frames, not a stub.

## Next lesson

[Lesson 09 — Receive photo from Telegram](09-receive-photo.md)
