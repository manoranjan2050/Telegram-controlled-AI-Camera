# Lesson 09 — Video Recording (file: `08-video.md`, Phase 9)

> Note on numbering: this file keeps the name `08-video.md` from the original
> repository layout (spec §5), even though it corresponds to build Phase 9 —
> the lesson-file list and the phase list in the master spec don't share the
> same numbering, and file names were kept stable rather than renumbered.

## Goal

Add `/video [seconds]`, recording to `/sdcard/videos/` and uploading to
Telegram, without blocking command handling for other chats while it runs.

## ⚠️ Hardware verification status

Same caveat as Phase 5/6: ESP32-P4 does have a hardware H.264 encoder block in
silicon, but whether (and how) it's usable depends on the still-unverified
camera/sensor driver from Phase 5. `telegramp4_video_record()` is an honest
stub returning `ESP_ERR_NOT_SUPPORTED` — **do not assume H.264 or any other
format works until tested end-to-end on real hardware.**

## What you learn

- Why long-running operations (recording) must run on their own FreeRTOS task,
  never on the Telegram long-poll task — otherwise every other chat's commands
  stall for the duration of the recording
- Passing per-invocation data (`chat_id`, `duration_s`) into a task via a
  heap-allocated argument struct that the task itself frees
- Clamping a user-supplied duration to configured min/max instead of trusting
  Telegram input directly

## Code

- [`components/telegramp4_video/`](../../components/telegramp4_video/) —
  `telegramp4_video_record()` abstraction (stub pending hardware verification)
- [`main/Kconfig.projbuild`](../../main/Kconfig.projbuild) — new `Video` submenu: min/
  default/max duration (1/10/60s)
- [`main/app_main.cpp`](../../main/app_main.cpp) — `handler_video()` spawns
  `video_record_task()`, which sends the "Recording..." message, calls
  `telegramp4_video_record()`, and uploads the result via
  `telegramp4_telegram_send_document()` on success

## How it works

`handler_video()` parses the optional `[seconds]` argument (falling back to
the configured default), clamps it to `[TELEGRAMP4_VIDEO_MIN_DURATION_S,
TELEGRAMP4_VIDEO_MAX_DURATION_S]`, and immediately returns after spawning
`video_record_task` — this keeps the Telegram poll task free to keep handling
other commands/chats while a (future, real) multi-second recording runs.
The task itself owns its heap-allocated argument struct and frees it before
`vTaskDelete(NULL)`.

Today, `telegramp4_video_record()` immediately returns
`ESP_ERR_NOT_SUPPORTED`, so the task replies `❌ Camera unavailable.` — correct,
honest behavior until Phase 5/9's hardware verification is done.

## Test

```bash
idf.py build
idf.py -p <PORT> flash monitor
```
`/video 5` in Telegram. Expected today: `🎥 Recording for 5 seconds...` followed
by `❌ Camera unavailable.`

## Next lesson

[Lesson 09 — Receive photo from Telegram](09-receive-photo.md)
